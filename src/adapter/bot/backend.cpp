#include "adapter/bot/backend.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <optional>
#include <unordered_map>
#include <vector>

#include <QStringList>

#include "core/game/rules.h"

namespace nenoserpent::adapter::bot {

namespace {

constexpr std::array<QPoint, 4> kDirections = {
  QPoint{0, -1},
  QPoint{0, 1},
  QPoint{-1, 0},
  QPoint{1, 0},
};

auto isReverseDirection(const QPoint& a, const QPoint& b) -> bool {
  return a.x() == -b.x() && a.y() == -b.y();
}

auto boardIndex(const QPoint& p, const int width) -> int {
  return p.y() * width + p.x();
}

auto isInsideBoard(const QPoint& p, const int width, const int height) -> bool {
  return p.x() >= 0 && p.y() >= 0 && p.x() < width && p.y() < height;
}

auto tryBoardIndex(const QPoint& p, const int width, const int height)
  -> std::optional<std::size_t> {
  if (!isInsideBoard(p, width, height)) {
    return std::nullopt;
  }
  return static_cast<std::size_t>(boardIndex(p, width));
}

auto toroidalDistance(const QPoint& from, const QPoint& to, const int width, const int height)
  -> int {
  const int dx = std::abs(from.x() - to.x());
  const int dy = std::abs(from.y() - to.y());
  return std::min(dx, width - dx) + std::min(dy, height - dy);
}

struct MoveState {
  QPoint head{0, 0};
  QPoint direction{0, -1};
  std::deque<QPoint> body;
  int score = 0;
};

struct StageSignals {
  int snakeFillPermille = 0;
  int obstacleFillPermille = 0;
  bool lateGame = false;
  bool crowded = false;
};

auto clampInt(const int value, const int minValue, const int maxValue) -> int {
  return std::max(minValue, std::min(value, maxValue));
}

auto deriveStageSignals(const Snapshot& snapshot) -> StageSignals {
  const int boardCells = std::max(1, snapshot.boardWidth * snapshot.boardHeight);
  const int bodyCells = static_cast<int>(snapshot.body.size());
  const int obstacleCells = static_cast<int>(snapshot.obstacles.size());

  StageSignals stage{};
  stage.snakeFillPermille = (bodyCells * 1000) / boardCells;
  stage.obstacleFillPermille = (obstacleCells * 1000) / boardCells;
  stage.lateGame = snapshot.score >= 80 || stage.snakeFillPermille >= 280;
  stage.crowded = stage.snakeFillPermille >= 220 || stage.obstacleFillPermille >= 120;
  return stage;
}

auto stageAdjustedStrategy(const StrategyConfig& base, const Snapshot& snapshot) -> StrategyConfig {
  StrategyConfig adjusted = base;
  const StageSignals stage = deriveStageSignals(snapshot);
  const DecisionPolicy policy = decisionPolicyFromEnvironment();

  if (policy == DecisionPolicy::Conservative) {
    adjusted.safeNeighborWeight += 8;
    adjusted.openSpaceWeight += 2;
    adjusted.trapPenalty += 20;
    adjusted.targetDistanceWeight = std::max(0, adjusted.targetDistanceWeight - 2);
    adjusted.foodConsumeBonus = std::max(0, adjusted.foodConsumeBonus - 4);
    adjusted.straightBonus = std::max(0, adjusted.straightBonus - 2);
    adjusted.lookaheadDepth += 1;
    adjusted.powerTargetDistanceSlack = std::max(0, adjusted.powerTargetDistanceSlack - 1);
  } else if (policy == DecisionPolicy::Aggressive) {
    adjusted.safeNeighborWeight = std::max(0, adjusted.safeNeighborWeight - 6);
    adjusted.openSpaceWeight = std::max(0, adjusted.openSpaceWeight - 1);
    adjusted.trapPenalty = std::max(0, adjusted.trapPenalty - 14);
    adjusted.targetDistanceWeight += 2;
    adjusted.foodConsumeBonus += 8;
    adjusted.straightBonus += 4;
    adjusted.powerTargetDistanceSlack += 2;
  }

  if (stage.lateGame) {
    adjusted.safeNeighborWeight += 7;
    adjusted.openSpaceWeight += 3;
    adjusted.trapPenalty += 18;
    adjusted.targetDistanceWeight = std::max(0, adjusted.targetDistanceWeight - 2);
    adjusted.powerTargetDistanceSlack = std::max(1, adjusted.powerTargetDistanceSlack - 1);
    adjusted.lookaheadDepth = std::max(adjusted.lookaheadDepth, 3);
  }

  if (stage.crowded) {
    adjusted.safeNeighborWeight += 4;
    adjusted.trapPenalty += 14;
    adjusted.targetDistanceWeight = std::max(0, adjusted.targetDistanceWeight - 1);
    adjusted.lookaheadDepth = std::max(adjusted.lookaheadDepth, 3);
  }

  if (snapshot.score < 20 && stage.snakeFillPermille < 140 && !stage.crowded) {
    adjusted.foodConsumeBonus += 4;
    adjusted.straightBonus += 2;
    if (adjusted.targetDistanceWeight > 0) {
      adjusted.targetDistanceWeight += 1;
    }
  }

  adjusted.openSpaceWeight = clampInt(adjusted.openSpaceWeight, 0, 60);
  adjusted.safeNeighborWeight = clampInt(adjusted.safeNeighborWeight, 0, 90);
  adjusted.targetDistanceWeight = clampInt(adjusted.targetDistanceWeight, 0, 80);
  adjusted.foodConsumeBonus = clampInt(adjusted.foodConsumeBonus, 0, 100);
  adjusted.trapPenalty = clampInt(adjusted.trapPenalty, 0, 160);
  adjusted.lookaheadDepth = clampInt(adjusted.lookaheadDepth, 0, 6);
  adjusted.powerTargetDistanceSlack = clampInt(adjusted.powerTargetDistanceSlack, 0, 20);
  return adjusted;
}

struct MovePreview {
  bool valid = false;
  MoveState next;
  bool ateFood = false;
  bool atePower = false;
};

enum class FilterRejectReason {
  Invalid,
  SafeNeighbors,
  OpenSpace,
  TailReachability,
};

struct CandidateStats {
  QPoint candidate{0, 0};
  MovePreview preview;
  std::vector<bool> blocked;
  int openSpace = 0;
  int safeNeighbors = 0;
  int revisitCount = 0;
  bool tailReachable = false;
};

struct ScoreBreakdown {
  int progress = 0;
  int survival = 0;
  int reward = 0;
  int risk = 0;
  int loopCost = 0;
  int drift = 0;

  [[nodiscard]] auto total() const -> int {
    return progress + survival + reward + drift - risk - loopCost;
  }
};

struct FilterStats {
  int legal = 0;
  int strictAccepted = 0;
  int strictSafeReject = 0;
  int strictSpaceReject = 0;
  int strictTailReject = 0;
};

struct CandidateTelemetry {
  QPoint direction{0, 0};
  ScoreBreakdown breakdown;
  int total = std::numeric_limits<int>::min();
};

auto mixHash(std::uint64_t seed, const std::uint64_t value) -> std::uint64_t {
  constexpr std::uint64_t kPrime = 1099511628211ULL;
  seed ^= value + 0x9e3779b97f4a7c15ULL + (seed << 6U) + (seed >> 2U);
  seed *= kPrime;
  return seed;
}

auto stateHash(const Snapshot& snapshot, const MoveState& state) -> std::uint64_t {
  std::uint64_t hash = 1469598103934665603ULL;
  hash = mixHash(hash, static_cast<std::uint64_t>(snapshot.boardWidth));
  hash = mixHash(hash, static_cast<std::uint64_t>(snapshot.boardHeight));
  hash = mixHash(hash, static_cast<std::uint64_t>(state.head.x() + 1024));
  hash = mixHash(hash, static_cast<std::uint64_t>(state.head.y() + 1024));
  hash = mixHash(hash, static_cast<std::uint64_t>(state.direction.x() + 16));
  hash = mixHash(hash, static_cast<std::uint64_t>(state.direction.y() + 16));
  hash = mixHash(hash, static_cast<std::uint64_t>(snapshot.food.x() + 1024));
  hash = mixHash(hash, static_cast<std::uint64_t>(snapshot.food.y() + 1024));
  hash = mixHash(hash, static_cast<std::uint64_t>(snapshot.powerUpPos.x() + 1024));
  hash = mixHash(hash, static_cast<std::uint64_t>(snapshot.powerUpPos.y() + 1024));
  hash = mixHash(hash, static_cast<std::uint64_t>(snapshot.powerUpType + 32));
  hash = mixHash(hash, static_cast<std::uint64_t>(state.body.size()));
  for (const QPoint& segment : state.body) {
    hash = mixHash(hash, static_cast<std::uint64_t>(segment.x() + 1024));
    hash = mixHash(hash, static_cast<std::uint64_t>(segment.y() + 1024));
  }
  return hash;
}

class LoopMemory {
public:
  auto clear() -> void {
    m_recent.clear();
    m_counts.clear();
    m_observeTick = 0;
  }

  auto observe(const Snapshot& snapshot, const MoveState& state) -> int {
    const std::uint64_t hash = stateHash(snapshot, state);
    const int repeats = ++m_counts[hash];
    m_recent.push_back(hash);
    trim();
    ++m_observeTick;
    return repeats;
  }

  [[nodiscard]] auto repeatsFor(const Snapshot& snapshot, const MoveState& state) const -> int {
    const std::uint64_t hash = stateHash(snapshot, state);
    const auto it = m_counts.find(hash);
    return it == m_counts.end() ? 0 : it->second;
  }

  [[nodiscard]] auto observeTick() const -> std::uint64_t {
    return m_observeTick;
  }

private:
  static constexpr int kWindow = 96;
  std::deque<std::uint64_t> m_recent;
  std::unordered_map<std::uint64_t, int> m_counts;
  std::uint64_t m_observeTick = 0;

  auto trim() -> void {
    while (static_cast<int>(m_recent.size()) > kWindow) {
      const std::uint64_t dropped = m_recent.front();
      m_recent.pop_front();
      auto it = m_counts.find(dropped);
      if (it == m_counts.end()) {
        continue;
      }
      --it->second;
      if (it->second <= 0) {
        m_counts.erase(it);
      }
    }
  }
};

class LoopController {
public:
  auto clear() -> void {
    m_lastScore = 0;
    m_noScoreTicks = 0;
    m_hasScore = false;
    m_escapeHistory.clear();
  }

  auto observeScore(const int score) -> void {
    if (!m_hasScore || score > m_lastScore) {
      m_noScoreTicks = 0;
    } else {
      ++m_noScoreTicks;
    }
    m_lastScore = score;
    m_hasScore = true;
  }

  [[nodiscard]] auto noScoreTicks() const -> int {
    return m_noScoreTicks;
  }

  [[nodiscard]] auto escapeMode(const int repeats) const -> bool {
    constexpr int kLoopRepeatThreshold = 4;
    constexpr int kNoScoreEscapeTicks = 28;
    return repeats >= kLoopRepeatThreshold ||
           (m_noScoreTicks >= kNoScoreEscapeTicks && repeats >= 2);
  }

  [[nodiscard]] auto loopCost(const int revisitCount,
                              const int pocketPenalty,
                              const StrategyConfig& config,
                              const bool useSearchScoring,
                              const bool escapeMode) const -> int {
    if (escapeMode) {
      return clampInt(revisitCount * config.loopEscapePenalty, 0, 420);
    }
    const int revisitPenalty =
      revisitCount *
      (useSearchScoring ? std::max(1, config.loopRepeatPenalty - 8) : config.loopRepeatPenalty);
    const int pocketScale = useSearchScoring ? 2 : 1;
    return clampInt(revisitPenalty + (pocketPenalty * pocketScale), 0, 360);
  }

  [[nodiscard]] auto escapeCycleContinuationPenalty(const QPoint& candidate,
                                                    const bool escapeMode,
                                                    const int noScoreTicks) const -> int {
    if (!escapeMode || noScoreTicks < 32 || m_escapeHistory.size() < 8) {
      return 0;
    }
    const auto n = static_cast<int>(m_escapeHistory.size());
    const bool twoCycle = (m_escapeHistory[n - 1] == m_escapeHistory[n - 3]) &&
                          (m_escapeHistory[n - 2] == m_escapeHistory[n - 4]);
    if (twoCycle && candidate == m_escapeHistory[n - 2]) {
      return 180 + ((noScoreTicks - 32) / 4);
    }
    const bool fourCycle = (m_escapeHistory[n - 1] == m_escapeHistory[n - 5]) &&
                           (m_escapeHistory[n - 2] == m_escapeHistory[n - 6]) &&
                           (m_escapeHistory[n - 3] == m_escapeHistory[n - 7]) &&
                           (m_escapeHistory[n - 4] == m_escapeHistory[n - 8]);
    if (fourCycle && candidate == m_escapeHistory[n - 4]) {
      return 160 + ((noScoreTicks - 32) / 5);
    }
    return 0;
  }

  auto observeEscapeDecision(const std::optional<QPoint>& direction, const bool escapeMode)
    -> void {
    if (!escapeMode || !direction.has_value()) {
      return;
    }
    m_escapeHistory.push_back(*direction);
    while (m_escapeHistory.size() > 12) {
      m_escapeHistory.pop_front();
    }
  }

private:
  int m_lastScore = 0;
  int m_noScoreTicks = 0;
  bool m_hasScore = false;
  std::deque<QPoint> m_escapeHistory;
};

enum class TargetMode {
  FoodChase,
  PowerChase,
  Escape,
};

class TargetFsm {
public:
  auto clear() -> void {
    m_mode = TargetMode::FoodChase;
    m_modeTicks = 0;
    m_lastObserveTick = 0;
    m_forceTailChaseTicks = 0;
    m_powerChaseCooldownTicks = 0;
  }

  auto mode() const -> TargetMode {
    return m_mode;
  }

  [[nodiscard]] auto suppressPowerChase() const -> bool {
    return m_powerChaseCooldownTicks > 0 || m_forceTailChaseTicks > 0;
  }

  [[nodiscard]] auto forceTailChaseActive() const -> bool {
    return m_forceTailChaseTicks > 0;
  }

  auto update(const Snapshot& snapshot,
              const StrategyConfig& config,
              const int repeats,
              const int noScoreTicks,
              const std::uint64_t observeTick) -> void {
    if (observeTick != m_lastObserveTick) {
      ++m_modeTicks;
      m_lastObserveTick = observeTick;
      if (m_forceTailChaseTicks > 0) {
        --m_forceTailChaseTicks;
      }
      if (m_powerChaseCooldownTicks > 0) {
        --m_powerChaseCooldownTicks;
      }
    }

    if (noScoreTicks >= 56) {
      m_forceTailChaseTicks = std::max(m_forceTailChaseTicks, 24);
      m_powerChaseCooldownTicks = std::max(m_powerChaseCooldownTicks, 40);
    }

    const bool hasPowerCandidate =
      snapshot.powerUpPos.x() >= 0 && snapshot.powerUpPos.y() >= 0 &&
      powerPriority(config, snapshot.powerUpType) >= config.powerTargetPriorityThreshold;
    const bool hasPower = hasPowerCandidate && !suppressPowerChase();
    const bool wantEscape = repeats >= 4 || noScoreTicks >= 28 || m_forceTailChaseTicks > 0;
    const TargetMode desired =
      wantEscape ? TargetMode::Escape : (hasPower ? TargetMode::PowerChase : TargetMode::FoodChase);

    if (desired == m_mode) {
      return;
    }
    if (desired == TargetMode::Escape) {
      switchMode(desired);
      return;
    }
    if (m_mode == TargetMode::Escape) {
      if (m_modeTicks >= 6) {
        switchMode(desired);
      }
      return;
    }
    if (m_modeTicks >= 10) {
      switchMode(desired);
    }
  }

  auto targetPoint(const Snapshot& snapshot, const StrategyConfig& config) const -> QPoint {
    if (m_forceTailChaseTicks > 0) {
      return snapshot.body.empty() ? snapshot.food : snapshot.body.back();
    }
    if (m_mode == TargetMode::Escape) {
      return snapshot.body.empty() ? snapshot.food : snapshot.body.back();
    }
    if (m_mode == TargetMode::PowerChase && snapshot.powerUpPos.x() >= 0 &&
        snapshot.powerUpPos.y() >= 0 &&
        powerPriority(config, snapshot.powerUpType) >= config.powerTargetPriorityThreshold) {
      return snapshot.powerUpPos;
    }
    return snapshot.food;
  }

private:
  auto switchMode(const TargetMode mode) -> void {
    m_mode = mode;
    m_modeTicks = 0;
  }

  TargetMode m_mode = TargetMode::FoodChase;
  int m_modeTicks = 0;
  int m_forceTailChaseTicks = 0;
  int m_powerChaseCooldownTicks = 0;
  std::uint64_t m_lastObserveTick = 0;
};

auto targetModeName(const TargetMode mode) -> QString {
  switch (mode) {
  case TargetMode::FoodChase:
    return QStringLiteral("FoodChase");
  case TargetMode::PowerChase:
    return QStringLiteral("PowerChase");
  case TargetMode::Escape:
    return QStringLiteral("Escape");
  }
  return QStringLiteral("Unknown");
}

auto buildBlockedMap(const Snapshot& snapshot, const std::deque<QPoint>& body)
  -> std::vector<bool> {
  std::vector<bool> blocked(static_cast<std::size_t>(snapshot.boardWidth * snapshot.boardHeight),
                            false);
  if (!snapshot.portalActive && !snapshot.laserActive) {
    for (const QPoint& obstacle : snapshot.obstacles) {
      if (const auto index = tryBoardIndex(obstacle, snapshot.boardWidth, snapshot.boardHeight);
          index.has_value()) {
        blocked[*index] = true;
      }
    }
  }
  if (!snapshot.ghostActive) {
    for (const QPoint& segment : body) {
      if (const auto index = tryBoardIndex(segment, snapshot.boardWidth, snapshot.boardHeight);
          index.has_value()) {
        blocked[*index] = true;
      }
    }
  }
  return blocked;
}

auto directionIndex(const QPoint& direction) -> int {
  for (int i = 0; i < static_cast<int>(kDirections.size()); ++i) {
    if (kDirections[static_cast<std::size_t>(i)] == direction) {
      return i;
    }
  }
  return 0;
}

auto floodReachable(const QPoint& start, const Snapshot& snapshot, const std::vector<bool>& blocked)
  -> int {
  if (snapshot.boardWidth <= 0 || snapshot.boardHeight <= 0) {
    return 0;
  }
  std::vector<bool> visited(blocked.size(), false);
  std::deque<QPoint> queue;
  const QPoint wrappedStart =
    nenoserpent::core::wrapPoint(start, snapshot.boardWidth, snapshot.boardHeight);
  const auto startIndex = tryBoardIndex(wrappedStart, snapshot.boardWidth, snapshot.boardHeight);
  if (!startIndex.has_value()) {
    return 0;
  }
  queue.push_back(wrappedStart);
  visited[*startIndex] = true;
  int reachable = 0;
  while (!queue.empty()) {
    const QPoint current = queue.front();
    queue.pop_front();
    ++reachable;
    for (const QPoint& dir : kDirections) {
      const QPoint next =
        nenoserpent::core::wrapPoint(current + dir, snapshot.boardWidth, snapshot.boardHeight);
      const auto nextIndex = tryBoardIndex(next, snapshot.boardWidth, snapshot.boardHeight);
      if (!nextIndex.has_value() || visited[*nextIndex] || blocked[*nextIndex]) {
        continue;
      }
      visited[*nextIndex] = true;
      queue.push_back(next);
    }
  }
  return reachable;
}

auto countSafeNeighbors(const QPoint& from,
                        const Snapshot& snapshot,
                        const std::vector<bool>& blocked) -> int {
  int safe = 0;
  for (const QPoint& dir : kDirections) {
    const QPoint next =
      nenoserpent::core::wrapPoint(from + dir, snapshot.boardWidth, snapshot.boardHeight);
    const auto index = tryBoardIndex(next, snapshot.boardWidth, snapshot.boardHeight);
    if (index.has_value() && !blocked[*index]) {
      ++safe;
    }
  }
  return safe;
}

auto shortestReachableDistance(const QPoint& from,
                               const QPoint& to,
                               const Snapshot& snapshot,
                               const std::vector<bool>& blocked) -> std::optional<int> {
  if (snapshot.boardWidth <= 0 || snapshot.boardHeight <= 0) {
    return std::nullopt;
  }
  const auto fromIndex = tryBoardIndex(from, snapshot.boardWidth, snapshot.boardHeight);
  const auto toIndex = tryBoardIndex(to, snapshot.boardWidth, snapshot.boardHeight);
  if (!fromIndex.has_value() || !toIndex.has_value()) {
    return std::nullopt;
  }
  if (from == to) {
    return 0;
  }

  std::vector<int> distance(blocked.size(), -1);
  std::deque<QPoint> queue;
  queue.push_back(from);
  distance[*fromIndex] = 0;

  while (!queue.empty()) {
    const QPoint current = queue.front();
    queue.pop_front();
    const auto currentIndex = tryBoardIndex(current, snapshot.boardWidth, snapshot.boardHeight);
    if (!currentIndex.has_value()) {
      continue;
    }
    const int nextDistance = distance[*currentIndex] + 1;
    for (const QPoint& dir : kDirections) {
      const QPoint next =
        nenoserpent::core::wrapPoint(current + dir, snapshot.boardWidth, snapshot.boardHeight);
      const auto nextIndex = tryBoardIndex(next, snapshot.boardWidth, snapshot.boardHeight);
      if (!nextIndex.has_value() || blocked[*nextIndex] || distance[*nextIndex] >= 0) {
        continue;
      }
      if (next == to) {
        return nextDistance;
      }
      distance[*nextIndex] = nextDistance;
      queue.push_back(next);
    }
  }
  return std::nullopt;
}

struct TargetDistance {
  int distance = 0;
  int unreachablePenalty = 0;
};

auto resolveTargetDistance(const QPoint& head,
                           const QPoint& target,
                           const Snapshot& snapshot,
                           const std::vector<bool>& blocked,
                           const QPoint& tailFallback) -> TargetDistance {
  if (const auto reachable = shortestReachableDistance(head, target, snapshot, blocked);
      reachable.has_value()) {
    return {.distance = *reachable, .unreachablePenalty = 0};
  }

  if (target != snapshot.food) {
    if (const auto foodReachable =
          shortestReachableDistance(head, snapshot.food, snapshot, blocked);
        foodReachable.has_value()) {
      return {.distance = *foodReachable, .unreachablePenalty = 64};
    }
  }

  if (const auto tailReachable = shortestReachableDistance(head, tailFallback, snapshot, blocked);
      tailReachable.has_value()) {
    return {.distance = *tailReachable, .unreachablePenalty = 96};
  }

  const int fallbackDistance =
    toroidalDistance(head, target, snapshot.boardWidth, snapshot.boardHeight);
  return {.distance = fallbackDistance, .unreachablePenalty = 180};
}

auto pocketPenaltyTowardTarget(const QPoint& from,
                               const QPoint& target,
                               const Snapshot& snapshot,
                               const std::vector<bool>& blocked) -> int {
  if (snapshot.boardWidth <= 0 || snapshot.boardHeight <= 0) {
    return 0;
  }
  const auto fromIndex = tryBoardIndex(from, snapshot.boardWidth, snapshot.boardHeight);
  const auto targetIndex = tryBoardIndex(target, snapshot.boardWidth, snapshot.boardHeight);
  if (!fromIndex.has_value() || !targetIndex.has_value()) {
    return 0;
  }
  if (from == target) {
    return 0;
  }

  std::vector<int> distance(blocked.size(), -1);
  std::vector<int> parent(blocked.size(), -1);
  std::deque<QPoint> queue;
  queue.push_back(from);
  distance[static_cast<std::size_t>(*fromIndex)] = 0;
  bool reached = false;

  while (!queue.empty() && !reached) {
    const QPoint current = queue.front();
    queue.pop_front();
    const auto currentIndex = tryBoardIndex(current, snapshot.boardWidth, snapshot.boardHeight);
    if (!currentIndex.has_value()) {
      continue;
    }
    const int nextDistance = distance[static_cast<std::size_t>(*currentIndex)] + 1;
    for (const QPoint& dir : kDirections) {
      const QPoint next =
        nenoserpent::core::wrapPoint(current + dir, snapshot.boardWidth, snapshot.boardHeight);
      const auto nextIndex = tryBoardIndex(next, snapshot.boardWidth, snapshot.boardHeight);
      if (!nextIndex.has_value()) {
        continue;
      }
      const auto idx = static_cast<std::size_t>(*nextIndex);
      if (blocked[idx] || distance[idx] >= 0) {
        continue;
      }
      distance[idx] = nextDistance;
      parent[idx] = *currentIndex;
      if (next == target) {
        reached = true;
        break;
      }
      queue.push_back(next);
    }
  }

  if (distance[static_cast<std::size_t>(*targetIndex)] < 0) {
    return 24;
  }

  int penalty = 0;
  int cursor = *targetIndex;
  while (cursor >= 0 && cursor != static_cast<int>(*fromIndex)) {
    const QPoint point(cursor % snapshot.boardWidth, cursor / snapshot.boardWidth);
    const int safeNeighbors = countSafeNeighbors(point, snapshot, blocked);
    if (safeNeighbors <= 1) {
      penalty += 30;
    } else if (safeNeighbors == 2) {
      penalty += 8;
    }
    cursor = parent[static_cast<std::size_t>(cursor)];
  }
  return penalty;
}

auto previewMove(const Snapshot& snapshot, const MoveState& state, const QPoint& candidate)
  -> MovePreview {
  MovePreview preview{};
  if (isReverseDirection(candidate, state.direction)) {
    return preview;
  }

  const QPoint nextHeadRaw = state.head + candidate;
  const QPoint wrappedHead =
    nenoserpent::core::wrapPoint(nextHeadRaw, snapshot.boardWidth, snapshot.boardHeight);
  std::deque<QPoint> collisionBody = state.body;
  const bool ateFood = wrappedHead == snapshot.food;
  const bool atePower = snapshot.powerUpPos.x() >= 0 && snapshot.powerUpPos.y() >= 0 &&
                        wrappedHead == snapshot.powerUpPos;

  if (!ateFood && !collisionBody.empty()) {
    collisionBody.pop_back();
  }
  const auto collision = nenoserpent::core::collisionOutcomeForHead(nextHeadRaw,
                                                                    snapshot.boardWidth,
                                                                    snapshot.boardHeight,
                                                                    snapshot.obstacles,
                                                                    collisionBody,
                                                                    snapshot.ghostActive,
                                                                    snapshot.portalActive,
                                                                    snapshot.laserActive,
                                                                    snapshot.shieldActive);
  if (collision.collision) {
    return preview;
  }

  collisionBody.push_front(wrappedHead);
  preview.next = {
    .head = wrappedHead,
    .direction = candidate,
    .body = std::move(collisionBody),
    .score = state.score + (ateFood ? 1 : 0),
  };
  preview.valid = true;
  preview.ateFood = ateFood;
  preview.atePower = atePower;
  return preview;
}

auto evaluateLeaf(const Snapshot& snapshot,
                  const MoveState& state,
                  const StrategyConfig& config,
                  const QPoint& target) -> int {
  auto blocked = buildBlockedMap(snapshot, state.body);
  if (const auto headIndex = tryBoardIndex(state.head, snapshot.boardWidth, snapshot.boardHeight);
      headIndex.has_value()) {
    blocked[*headIndex] = false;
  }
  const int openSpace = floodReachable(state.head, snapshot, blocked);
  const int safeNeighbors = countSafeNeighbors(state.head, snapshot, blocked);
  const QPoint tailFallback = state.body.empty() ? state.head : state.body.back();
  const TargetDistance targetDistance =
    resolveTargetDistance(state.head, target, snapshot, blocked, tailFallback);
  const int trapPenalty = safeNeighbors <= 1 ? config.trapPenalty : 0;
  return (openSpace * config.openSpaceWeight) + (safeNeighbors * config.safeNeighborWeight) -
         (targetDistance.distance * config.targetDistanceWeight) - trapPenalty +
         (state.score * 48) - targetDistance.unreachablePenalty;
}

auto searchValue(const Snapshot& snapshot,
                 const MoveState& state,
                 const StrategyConfig& config,
                 const int depth,
                 const QPoint& target) -> int {
  if (depth <= 0) {
    return evaluateLeaf(snapshot, state, config, target);
  }

  int best = std::numeric_limits<int>::min();
  bool hasMove = false;
  for (const QPoint& candidate : kDirections) {
    const auto preview = previewMove(snapshot, state, candidate);
    if (!preview.valid) {
      continue;
    }
    hasMove = true;
    int immediate = (candidate == state.direction ? config.straightBonus : 0);
    if (preview.ateFood) {
      immediate += config.foodConsumeBonus;
    }
    if (preview.atePower) {
      immediate += powerPriority(config, snapshot.powerUpType);
    }
    const int score = immediate + searchValue(snapshot, preview.next, config, depth - 1, target);
    if (score > best) {
      best = score;
    }
  }

  if (!hasMove) {
    return std::numeric_limits<int>::min() / 2;
  }
  return best;
}

auto rolloutHorizon(const StrategyConfig& config) -> int {
  return clampInt(8 + (config.lookaheadDepth * 2), 8, 20);
}

auto rolloutScore(const Snapshot& snapshot,
                  const MoveState& startState,
                  const StrategyConfig& config,
                  const QPoint& target) -> int {
  MoveState current = startState;
  int total = 0;
  const int horizon = rolloutHorizon(config);

  for (int step = 0; step < horizon; ++step) {
    int best = std::numeric_limits<int>::min();
    std::optional<MovePreview> bestPreview;
    for (const QPoint& candidate : kDirections) {
      const auto preview = previewMove(snapshot, current, candidate);
      if (!preview.valid) {
        continue;
      }
      int score = evaluateLeaf(snapshot, preview.next, config, target);
      if (preview.ateFood) {
        score += config.foodConsumeBonus * 2;
      }
      if (preview.atePower) {
        score += powerPriority(config, snapshot.powerUpType);
      }
      if (score > best) {
        best = score;
        bestPreview = preview;
      }
    }

    if (!bestPreview.has_value()) {
      total -= 400;
      break;
    }

    current = bestPreview->next;
    total += best;
  }

  total += current.score * 32;
  return total;
}

auto evaluateEscapeCandidate(const Snapshot& snapshot,
                             const MovePreview& preview,
                             const StrategyConfig& config,
                             const int revisitCount) -> int {
  auto blocked = buildBlockedMap(snapshot, preview.next.body);
  if (const auto headIndex =
        tryBoardIndex(preview.next.head, snapshot.boardWidth, snapshot.boardHeight);
      headIndex.has_value()) {
    blocked[*headIndex] = false;
  }
  const int openSpace = floodReachable(preview.next.head, snapshot, blocked);
  const int safeNeighbors = countSafeNeighbors(preview.next.head, snapshot, blocked);
  const QPoint tail = preview.next.body.empty() ? preview.next.head : preview.next.body.back();
  const int tailDistance =
    toroidalDistance(preview.next.head, tail, snapshot.boardWidth, snapshot.boardHeight);

  int score = (openSpace * config.openSpaceWeight * 2) +
              (safeNeighbors * config.safeNeighborWeight * 3) + (tailDistance * 14) -
              (revisitCount * config.loopEscapePenalty);
  if (preview.ateFood) {
    score += config.foodConsumeBonus * 6;
  }
  if (preview.atePower) {
    score += powerPriority(config, snapshot.powerUpType) * 4;
  }
  return score;
}

auto boardCells(const Snapshot& snapshot) -> int {
  return std::max(1, snapshot.boardWidth * snapshot.boardHeight);
}

auto riskBudgetFor(const Snapshot& snapshot, const int repeats) -> int {
  const DecisionPolicy policy = decisionPolicyFromEnvironment();
  const int fillPermille = (static_cast<int>(snapshot.body.size()) * 1000) / boardCells(snapshot);
  const int obstaclePermille =
    (static_cast<int>(snapshot.obstacles.size()) * 1000) / boardCells(snapshot);

  int budget = 120;
  budget -= fillPermille / 10;
  budget -= obstaclePermille / 16;
  budget -= repeats * 8;
  if (snapshot.shieldActive) {
    budget += 14;
  }
  if (snapshot.ghostActive) {
    budget += 8;
  }
  if (policy == DecisionPolicy::Conservative) {
    budget -= 18;
  } else if (policy == DecisionPolicy::Aggressive) {
    budget += 16;
  }
  return clampInt(budget, 36, 140);
}

auto candidateRiskCost(const int openSpace,
                       const int safeNeighbors,
                       const int revisitCount,
                       const Snapshot& snapshot) -> int {
  const int cells = boardCells(snapshot);
  int risk = revisitCount * 10;
  if (safeNeighbors <= 1) {
    risk += 70;
  } else if (safeNeighbors == 2) {
    risk += 20;
  }
  if (openSpace < (cells / 8)) {
    risk += 50;
  } else if (openSpace < (cells / 5)) {
    risk += 20;
  }
  return risk;
}

auto approachTargetBonus(const QPoint& currentHead,
                         const QPoint& nextHead,
                         const QPoint& target,
                         const Snapshot& snapshot,
                         const StrategyConfig& config,
                         const int repeats) -> int {
  const int currentDistance =
    toroidalDistance(currentHead, target, snapshot.boardWidth, snapshot.boardHeight);
  const int nextDistance =
    toroidalDistance(nextHead, target, snapshot.boardWidth, snapshot.boardHeight);
  const int delta = currentDistance - nextDistance;
  int scale = config.targetDistanceWeight + (config.foodConsumeBonus / 2);
  if (target == snapshot.food && snapshot.score < 60 &&
      static_cast<int>(snapshot.body.size()) < 12) {
    scale += config.foodConsumeBonus / 2;
  }
  if (scale <= 0) {
    return 0;
  }
  scale *= 3;
  int bonus = delta * scale;
  if (delta <= 0 && repeats >= 2 && target == snapshot.food) {
    bonus -= 36;
  }
  return bonus;
}

auto clampScoreBlock(const int value, const int minValue, const int maxValue) -> int {
  return std::max(minValue, std::min(value, maxValue));
}

struct HardFilterConfig {
  int minSafeNeighbors = 1;
  int minOpenSpace = 0;
  bool requireTailReachable = false;
};

auto buildHardFilterConfig(const Snapshot& snapshot, const bool relaxed) -> HardFilterConfig {
  const bool early = snapshot.score < 50 && static_cast<int>(snapshot.body.size()) < 14;
  HardFilterConfig conf{};
  conf.minSafeNeighbors = early ? 2 : 1;
  conf.minOpenSpace = static_cast<int>(snapshot.body.size()) + (early ? 8 : 4);
  conf.requireTailReachable = early && !snapshot.ghostActive && !snapshot.shieldActive;
  if (relaxed) {
    conf.minSafeNeighbors = std::max(1, conf.minSafeNeighbors - 1);
    conf.minOpenSpace = std::max(0, conf.minOpenSpace - 6);
    conf.requireTailReachable = false;
  }
  return conf;
}

auto passesHardFilter(const CandidateStats& stats, const HardFilterConfig& conf)
  -> std::optional<FilterRejectReason> {
  if (!stats.preview.valid) {
    return FilterRejectReason::Invalid;
  }
  if (stats.safeNeighbors < conf.minSafeNeighbors) {
    return FilterRejectReason::SafeNeighbors;
  }
  if (stats.openSpace < conf.minOpenSpace) {
    return FilterRejectReason::OpenSpace;
  }
  if (conf.requireTailReachable && !stats.tailReachable) {
    return FilterRejectReason::TailReachability;
  }
  return std::nullopt;
}

auto selectLoopAwareDirection(const Snapshot& snapshot,
                              const StrategyConfig& config,
                              LoopMemory& memory,
                              LoopController& loopController,
                              TargetFsm& targetFsm,
                              QString* decisionSummaryOut,
                              const bool useSearchScoring) -> std::optional<QPoint> {
  if (snapshot.body.empty() || snapshot.boardWidth <= 0 || snapshot.boardHeight <= 0) {
    if (decisionSummaryOut != nullptr) {
      *decisionSummaryOut = QStringLiteral("bot decision: invalid snapshot");
    }
    return std::nullopt;
  }
  const StrategyConfig tunedConfig = stageAdjustedStrategy(config, snapshot);
  const MoveState initial{
    .head = snapshot.head,
    .direction = snapshot.direction,
    .body = snapshot.body,
    .score = snapshot.score,
  };

  const int repeats = memory.observe(snapshot, initial);
  loopController.observeScore(initial.score);
  const int noScoreTicks = loopController.noScoreTicks();
  targetFsm.update(snapshot, tunedConfig, repeats, noScoreTicks, memory.observeTick());
  const bool escapeMode =
    (targetFsm.mode() == TargetMode::Escape) || loopController.escapeMode(repeats);
  const int riskBudget = riskBudgetFor(snapshot, repeats);
  const int depth = std::clamp(tunedConfig.lookaheadDepth + 1, 2, 6);
  QPoint primaryTarget = targetFsm.targetPoint(snapshot, tunedConfig);
  if (escapeMode && noScoreTicks >= 72) {
    const std::array<QPoint, 4> escapeAnchors = {
      QPoint(0, 0),
      QPoint(snapshot.boardWidth - 1, 0),
      QPoint(snapshot.boardWidth - 1, snapshot.boardHeight - 1),
      QPoint(0, snapshot.boardHeight - 1),
    };
    const auto anchorIndex = static_cast<int>((memory.observeTick() / 8U) %
                                              static_cast<std::uint64_t>(escapeAnchors.size()));
    primaryTarget = escapeAnchors[static_cast<std::size_t>(anchorIndex)];
  }
  const int currentPrimaryDistance =
    toroidalDistance(initial.head, primaryTarget, snapshot.boardWidth, snapshot.boardHeight);
  const bool earlyFoodChaseGuard = (targetFsm.mode() == TargetMode::FoodChase) &&
                                   (primaryTarget == snapshot.food) && snapshot.score < 40 &&
                                   static_cast<int>(snapshot.body.size()) < 12 && !escapeMode;

  std::vector<CandidateStats> legalCandidates;
  legalCandidates.reserve(kDirections.size());
  for (const QPoint& candidate : kDirections) {
    const auto preview = previewMove(snapshot, initial, candidate);
    if (!preview.valid) {
      continue;
    }
    CandidateStats stats{};
    stats.candidate = candidate;
    stats.preview = preview;
    stats.revisitCount = memory.repeatsFor(snapshot, preview.next);
    stats.blocked = buildBlockedMap(snapshot, preview.next.body);
    if (const auto headIndex =
          tryBoardIndex(preview.next.head, snapshot.boardWidth, snapshot.boardHeight);
        headIndex.has_value()) {
      stats.blocked[*headIndex] = false;
    }
    stats.openSpace = floodReachable(preview.next.head, snapshot, stats.blocked);
    stats.safeNeighbors = countSafeNeighbors(preview.next.head, snapshot, stats.blocked);
    const QPoint tailFallback =
      preview.next.body.empty() ? preview.next.head : preview.next.body.back();
    auto tailReachBlocked = stats.blocked;
    if (const auto tailIndex =
          tryBoardIndex(tailFallback, snapshot.boardWidth, snapshot.boardHeight);
        tailIndex.has_value()) {
      tailReachBlocked[*tailIndex] = false;
    }
    stats.tailReachable =
      shortestReachableDistance(preview.next.head, tailFallback, snapshot, tailReachBlocked)
        .has_value();
    legalCandidates.push_back(std::move(stats));
  }
  if (legalCandidates.empty()) {
    if (decisionSummaryOut != nullptr) {
      *decisionSummaryOut = QStringLiteral("bot decision: no legal candidates");
    }
    return std::nullopt;
  }
  FilterStats filterStats{};
  filterStats.legal = static_cast<int>(legalCandidates.size());

  auto filterCandidates = [&](const HardFilterConfig& conf, const bool collectStrictStats) {
    std::vector<CandidateStats*> accepted;
    accepted.reserve(legalCandidates.size());
    for (auto& candidate : legalCandidates) {
      const auto reason = passesHardFilter(candidate, conf);
      if (!reason.has_value()) {
        accepted.push_back(&candidate);
        if (collectStrictStats) {
          ++filterStats.strictAccepted;
        }
      } else if (collectStrictStats) {
        switch (*reason) {
        case FilterRejectReason::SafeNeighbors:
          ++filterStats.strictSafeReject;
          break;
        case FilterRejectReason::OpenSpace:
          ++filterStats.strictSpaceReject;
          break;
        case FilterRejectReason::TailReachability:
          ++filterStats.strictTailReject;
          break;
        case FilterRejectReason::Invalid:
          break;
        }
      }
    }
    return accepted;
  };

  std::vector<CandidateStats*> viable =
    filterCandidates(buildHardFilterConfig(snapshot, false), true);
  if (viable.empty()) {
    viable = filterCandidates(buildHardFilterConfig(snapshot, true), false);
  }
  if (viable.empty()) {
    for (auto& candidate : legalCandidates) {
      viable.push_back(&candidate);
    }
  }

  const int currentFoodDistance =
    earlyFoodChaseGuard
      ? toroidalDistance(initial.head, snapshot.food, snapshot.boardWidth, snapshot.boardHeight)
      : 0;
  bool hasNonWorseningFoodMove = false;
  if (earlyFoodChaseGuard) {
    for (const CandidateStats* candidate : viable) {
      const int nextFoodDistance = toroidalDistance(
        candidate->preview.next.head, snapshot.food, snapshot.boardWidth, snapshot.boardHeight);
      if (nextFoodDistance <= currentFoodDistance) {
        hasNonWorseningFoodMove = true;
        break;
      }
    }
  }

  const auto tieRotateSeed = static_cast<std::uint64_t>(stateHash(snapshot, initial)) ^
                             static_cast<std::uint64_t>(tunedConfig.tieBreakSeed) ^
                             memory.observeTick();
  const int tieRotateOffset =
    static_cast<int>(tieRotateSeed % static_cast<std::uint64_t>(kDirections.size()));
  const int orbitBreakLevel =
    escapeMode
      ? std::clamp(((std::max(0, noScoreTicks - 24)) / 10) + std::max(0, repeats - 2), 0, 8)
      : 0;
  const bool deepEscapeStall = escapeMode && (noScoreTicks >= 96);
  const int orbitPreferredIndex =
    orbitBreakLevel > 0
      ? static_cast<int>((tieRotateSeed + static_cast<std::uint64_t>(noScoreTicks / 3U)) %
                         static_cast<std::uint64_t>(kDirections.size()))
      : -1;

  int bestScore = std::numeric_limits<int>::min();
  int bestTieRank = std::numeric_limits<int>::max();
  std::optional<QPoint> bestDirection;
  std::vector<CandidateTelemetry> candidateTelemetry;
  candidateTelemetry.reserve(viable.size());
  for (const CandidateStats* candidateStats : viable) {
    const QPoint candidate = candidateStats->candidate;
    const int candidateIndex = directionIndex(candidate);
    const MovePreview& preview = candidateStats->preview;
    const int revisitCount = candidateStats->revisitCount;
    const int openSpace = candidateStats->openSpace;
    const int safeNeighbors = candidateStats->safeNeighbors;
    const auto& blocked = candidateStats->blocked;
    const int pocketPenalty =
      pocketPenaltyTowardTarget(preview.next.head, primaryTarget, snapshot, blocked);
    const int boardArea = std::max(1, snapshot.boardWidth * snapshot.boardHeight);
    const int openSpacePct = (openSpace * 100) / boardArea;
    const int normalizedSafeNeighbors = safeNeighbors * 20;
    ScoreBreakdown breakdown{};
    if (escapeMode) {
      const int escapeBase = evaluateEscapeCandidate(snapshot, preview, tunedConfig, revisitCount);
      const int compressedEscapeBase = (escapeBase * 3) / 10;
      const int openSpaceTerm = (openSpacePct * 7) / 4;
      const int safeNeighborTerm = safeNeighbors * 22;
      const int tailReachTerm = candidateStats->tailReachable ? 14 : -42;
      const int stallDecay = std::min(120, std::max(0, noScoreTicks - 20) * 3);
      const int escapeSurvival =
        compressedEscapeBase + openSpaceTerm + safeNeighborTerm + tailReachTerm - stallDecay;
      breakdown.survival = clampScoreBlock(escapeSurvival, -60, 190);
      breakdown.progress = clampScoreBlock(
        approachTargetBonus(
          initial.head, preview.next.head, primaryTarget, snapshot, tunedConfig, repeats),
        -80,
        120);
      if (preview.ateFood) {
        breakdown.reward += tunedConfig.foodConsumeBonus * 2;
      }
      if (preview.atePower && !targetFsm.suppressPowerChase()) {
        breakdown.reward += powerPriority(tunedConfig, snapshot.powerUpType) * 2;
      }
      breakdown.reward = clampScoreBlock(breakdown.reward, 0, 240);
      if (orbitBreakLevel > 0) {
        if (orbitPreferredIndex >= 0 && candidate == kDirections[orbitPreferredIndex]) {
          breakdown.progress += orbitBreakLevel * 10;
        }
        if (candidate == snapshot.direction) {
          breakdown.progress -= orbitBreakLevel * 8;
        }
        const int straightIndex = directionIndex(snapshot.direction);
        if (straightIndex >= 0 &&
            candidateIndex == ((straightIndex + 2) % static_cast<int>(kDirections.size()))) {
          breakdown.progress -= orbitBreakLevel * 6;
        }
        breakdown.progress = clampScoreBlock(breakdown.progress, -120, 170);
      }
      const int repeatSquaredPenalty = std::min(260, revisitCount * revisitCount * 12);
      const int cyclePenalty =
        loopController.escapeCycleContinuationPenalty(candidate, true, noScoreTicks);
      const int escapeLoopExtra =
        std::min(320,
                 (revisitCount * 24) + repeatSquaredPenalty + (pocketPenalty * 10) +
                   (orbitBreakLevel * 18) + cyclePenalty + std::max(0, noScoreTicks - 24));
      breakdown.loopCost = clampScoreBlock(
        loopController.loopCost(revisitCount, pocketPenalty, tunedConfig, useSearchScoring, true) +
          escapeLoopExtra,
        0,
        520);
    } else if (useSearchScoring) {
      int immediate = (candidate == snapshot.direction ? tunedConfig.straightBonus : 0);
      if (preview.ateFood) {
        immediate += tunedConfig.foodConsumeBonus;
      }
      if (preview.atePower && !targetFsm.suppressPowerChase()) {
        immediate += powerPriority(tunedConfig, snapshot.powerUpType);
      }
      const QPoint tailFallback =
        preview.next.body.empty() ? preview.next.head : preview.next.body.back();
      const TargetDistance targetDistance =
        resolveTargetDistance(preview.next.head, primaryTarget, snapshot, blocked, tailFallback);
      const int searchTerm =
        searchValue(snapshot, preview.next, tunedConfig, depth - 1, primaryTarget);
      const int rolloutTerm = rolloutScore(snapshot, preview.next, tunedConfig, primaryTarget) / 6;
      breakdown.progress = clampScoreBlock(
        approachTargetBonus(
          initial.head, preview.next.head, primaryTarget, snapshot, tunedConfig, repeats) +
          (searchTerm / 8) + (rolloutTerm / 4) -
          (targetDistance.distance * tunedConfig.targetDistanceWeight) -
          targetDistance.unreachablePenalty,
        -160,
        190);
      breakdown.survival = clampScoreBlock(((openSpacePct * tunedConfig.openSpaceWeight) / 8) +
                                             normalizedSafeNeighbors +
                                             (safeNeighbors * tunedConfig.safeNeighborWeight) +
                                             (candidateStats->tailReachable ? 22 : -44),
                                           -120,
                                           170);
      breakdown.reward = clampScoreBlock(immediate, 0, 220);
      breakdown.loopCost =
        loopController.loopCost(revisitCount, pocketPenalty, tunedConfig, true, false);
    } else {
      const QPoint tailFallback =
        preview.next.body.empty() ? preview.next.head : preview.next.body.back();
      const TargetDistance targetDistance =
        resolveTargetDistance(preview.next.head, primaryTarget, snapshot, blocked, tailFallback);
      int immediate = (candidate == snapshot.direction ? tunedConfig.straightBonus : 0);
      if (preview.ateFood) {
        immediate += tunedConfig.foodConsumeBonus;
      }
      if (preview.atePower && !targetFsm.suppressPowerChase()) {
        immediate += powerPriority(tunedConfig, snapshot.powerUpType);
      }
      breakdown.progress = clampScoreBlock(
        approachTargetBonus(
          initial.head, preview.next.head, primaryTarget, snapshot, tunedConfig, repeats) -
          (targetDistance.distance * tunedConfig.targetDistanceWeight) -
          targetDistance.unreachablePenalty,
        -140,
        170);
      breakdown.survival = clampScoreBlock(((openSpacePct * tunedConfig.openSpaceWeight) / 8) +
                                             normalizedSafeNeighbors +
                                             (safeNeighbors * tunedConfig.safeNeighborWeight) +
                                             (candidateStats->tailReachable ? 20 : -44),
                                           -110,
                                           160);
      breakdown.reward = clampScoreBlock(immediate, 0, 220);
      breakdown.loopCost =
        loopController.loopCost(revisitCount, pocketPenalty, tunedConfig, false, false);
    }

    const int riskCost = candidateRiskCost(openSpace, safeNeighbors, revisitCount, snapshot);
    const int trapPenalty = safeNeighbors <= 1 ? tunedConfig.trapPenalty : 0;
    breakdown.risk = clampScoreBlock(trapPenalty + std::max(0, riskCost - riskBudget) * 6, 0, 320);
    if (snapshot.score < 50 && static_cast<int>(snapshot.body.size()) < 14) {
      if (safeNeighbors <= 2) {
        breakdown.risk = clampScoreBlock(breakdown.risk + ((3 - safeNeighbors) * 48), 0, 380);
      }
      if (openSpace < static_cast<int>(preview.next.body.size()) + 8) {
        breakdown.risk = clampScoreBlock(breakdown.risk + 56, 0, 380);
      }
    }
    int score = breakdown.total();
    if (deepEscapeStall) {
      const int anchorDistance = toroidalDistance(
        preview.next.head, primaryTarget, snapshot.boardWidth, snapshot.boardHeight);
      const int cyclePenalty =
        loopController.escapeCycleContinuationPenalty(candidate, true, noScoreTicks);
      const int directionalPenalty = candidate == snapshot.direction ? 50 : 0;
      const int deterministicJitter =
        static_cast<int>(((tieRotateSeed + static_cast<std::uint64_t>((candidateIndex + 1) * 17) +
                           static_cast<std::uint64_t>(noScoreTicks)) %
                          9ULL)) -
        4;
      const int deepEscapeScore =
        (openSpacePct * 4) + (safeNeighbors * 52) + (candidateStats->tailReachable ? 48 : -96) -
        (anchorDistance * 28) - (revisitCount * 64) - breakdown.loopCost - breakdown.risk -
        cyclePenalty - directionalPenalty + deterministicJitter;
      breakdown.progress = clampScoreBlock(deepEscapeScore, -520, 320);
      breakdown.survival = 0;
      breakdown.reward = 0;
      breakdown.drift = 0;
      breakdown.risk = 0;
      breakdown.loopCost = 0;
      score = breakdown.total();
    }
    if (!escapeMode && noScoreTicks >= 12) {
      const int nextPrimaryDistance = toroidalDistance(
        preview.next.head, primaryTarget, snapshot.boardWidth, snapshot.boardHeight);
      const int distanceDelta = nextPrimaryDistance - currentPrimaryDistance;
      if (distanceDelta > 0) {
        const int stallPenalty = std::min(180, distanceDelta * (8 + (noScoreTicks / 4)));
        breakdown.drift = -stallPenalty;
      } else if (distanceDelta < 0) {
        const int stallBonus = std::min(96, (-distanceDelta) * (6 + (noScoreTicks / 8)));
        breakdown.drift = stallBonus;
      }
      score = breakdown.total();
    }
    if (earlyFoodChaseGuard && hasNonWorseningFoodMove) {
      const int nextFoodDistance = toroidalDistance(
        preview.next.head, snapshot.food, snapshot.boardWidth, snapshot.boardHeight);
      if (nextFoodDistance > currentFoodDistance) {
        score -= 720;
      }
    }

    const int rawIndex = candidateIndex;
    int tieRank = (rawIndex - tieRotateOffset + static_cast<int>(kDirections.size())) %
                  static_cast<int>(kDirections.size());
    if (deepEscapeStall) {
      tieRank += loopController.escapeCycleContinuationPenalty(candidate, true, noScoreTicks) / 8;
    }
    if (score > bestScore || (score == bestScore && tieRank < bestTieRank)) {
      bestScore = score;
      bestTieRank = tieRank;
      bestDirection = candidate;
    }
    candidateTelemetry.push_back({.direction = candidate, .breakdown = breakdown, .total = score});
  }
  if (decisionSummaryOut != nullptr) {
    std::sort(candidateTelemetry.begin(),
              candidateTelemetry.end(),
              [](const CandidateTelemetry& lhs, const CandidateTelemetry& rhs) {
                return lhs.total > rhs.total;
              });
    const int topCount = std::min(3, static_cast<int>(candidateTelemetry.size()));
    QStringList topItems;
    topItems.reserve(topCount);
    for (int i = 0; i < topCount; ++i) {
      const auto& item = candidateTelemetry[static_cast<std::size_t>(i)];
      topItems.append(QStringLiteral("(%1,%2)=%3[p=%4 s=%5 r=%6 d=%7 rk=%8 lc=%9]")
                        .arg(item.direction.x())
                        .arg(item.direction.y())
                        .arg(item.total)
                        .arg(item.breakdown.progress)
                        .arg(item.breakdown.survival)
                        .arg(item.breakdown.reward)
                        .arg(item.breakdown.drift)
                        .arg(item.breakdown.risk)
                        .arg(item.breakdown.loopCost));
    }
    *decisionSummaryOut =
      QStringLiteral("bot decision: mode=%1 legal=%2 strict_ok=%3 reject{safe=%4 space=%5 tail=%6}"
                     " viable=%7 selected=(%8,%9) score=%10 top3=%11")
        .arg(targetModeName(targetFsm.mode()))
        .arg(filterStats.legal)
        .arg(filterStats.strictAccepted)
        .arg(filterStats.strictSafeReject)
        .arg(filterStats.strictSpaceReject)
        .arg(filterStats.strictTailReject)
        .arg(viable.size())
        .arg(bestDirection.has_value() ? bestDirection->x() : 0)
        .arg(bestDirection.has_value() ? bestDirection->y() : 0)
        .arg(bestScore)
        .arg(topItems.join(QStringLiteral(" ")));
  }
  loopController.observeEscapeDecision(bestDirection, escapeMode);
  return bestDirection;
}

class RuleBackend final : public BotBackend {
public:
  [[nodiscard]] auto name() const -> QString override {
    return QStringLiteral("rule");
  }

  [[nodiscard]] auto decideDirection(const Snapshot& snapshot, const StrategyConfig& config) const
    -> std::optional<QPoint> override {
    return selectLoopAwareDirection(
      snapshot, config, m_loopMemory, m_loopController, m_targetFsm, &m_lastDecisionSummary, false);
  }

  [[nodiscard]] auto decideChoice(const QVariantList& choices, const StrategyConfig& config) const
    -> int override {
    return pickChoiceIndex(choices, config);
  }

  [[nodiscard]] auto lastDecisionSummary() const -> QString override {
    return m_lastDecisionSummary;
  }

  void reset() override {
    m_loopMemory.clear();
    m_loopController.clear();
    m_targetFsm.clear();
    m_lastDecisionSummary.clear();
  }

private:
  mutable LoopMemory m_loopMemory;
  mutable LoopController m_loopController;
  mutable TargetFsm m_targetFsm;
  mutable QString m_lastDecisionSummary;
};

class SearchBackend final : public BotBackend {
public:
  [[nodiscard]] auto name() const -> QString override {
    return QStringLiteral("search");
  }

  [[nodiscard]] auto decideDirection(const Snapshot& snapshot, const StrategyConfig& config) const
    -> std::optional<QPoint> override {
    return selectLoopAwareDirection(
      snapshot, config, m_loopMemory, m_loopController, m_targetFsm, &m_lastDecisionSummary, true);
  }

  [[nodiscard]] auto decideChoice(const QVariantList& choices, const StrategyConfig& config) const
    -> int override {
    return pickChoiceIndex(choices, config);
  }

  [[nodiscard]] auto lastDecisionSummary() const -> QString override {
    return m_lastDecisionSummary;
  }

  void reset() override {
    m_loopMemory.clear();
    m_loopController.clear();
    m_targetFsm.clear();
    m_lastDecisionSummary.clear();
  }

private:
  mutable LoopMemory m_loopMemory;
  mutable LoopController m_loopController;
  mutable TargetFsm m_targetFsm;
  mutable QString m_lastDecisionSummary;
};

} // namespace

auto ruleBackend() -> const BotBackend& {
  static const RuleBackend kBackend;
  return kBackend;
}

auto searchBackend() -> const BotBackend& {
  static const SearchBackend kBackend;
  return kBackend;
}

} // namespace nenoserpent::adapter::bot
