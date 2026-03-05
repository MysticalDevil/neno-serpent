#include "adapter/bot/backend.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <optional>
#include <unordered_map>
#include <vector>

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
    m_lastScore = 0;
    m_noScoreTicks = 0;
    m_hasScore = false;
  }

  auto observe(const Snapshot& snapshot, const MoveState& state) -> int {
    const std::uint64_t hash = stateHash(snapshot, state);
    const int repeats = ++m_counts[hash];
    m_recent.push_back(hash);
    trim();
    ++m_observeTick;
    if (!m_hasScore || state.score > m_lastScore) {
      m_noScoreTicks = 0;
    } else {
      ++m_noScoreTicks;
    }
    m_lastScore = state.score;
    m_hasScore = true;
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

  [[nodiscard]] auto noScoreTicks() const -> int {
    return m_noScoreTicks;
  }

private:
  static constexpr int kWindow = 96;
  std::deque<std::uint64_t> m_recent;
  std::unordered_map<std::uint64_t, int> m_counts;
  std::uint64_t m_observeTick = 0;
  int m_lastScore = 0;
  int m_noScoreTicks = 0;
  bool m_hasScore = false;

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

auto resolvePrimaryTarget(const QPoint& head,
                          const Snapshot& snapshot,
                          const StrategyConfig& config) -> QPoint {
  QPoint target = snapshot.food;
  const bool hasPowerUp = snapshot.powerUpPos.x() >= 0 && snapshot.powerUpPos.y() >= 0;
  const int priority = powerPriority(config, snapshot.powerUpType);

  if (hasPowerUp && priority >= config.powerTargetPriorityThreshold) {
    const int foodDistance =
      toroidalDistance(head, snapshot.food, snapshot.boardWidth, snapshot.boardHeight);
    const int powerDistance =
      toroidalDistance(head, snapshot.powerUpPos, snapshot.boardWidth, snapshot.boardHeight);
    if (powerDistance <= foodDistance + config.powerTargetDistanceSlack) {
      target = snapshot.powerUpPos;
    }
  }
  return target;
}

auto resolveTargetDistance(const QPoint& head,
                           const Snapshot& snapshot,
                           const StrategyConfig& config,
                           const std::vector<bool>& blocked,
                           const QPoint& tailFallback) -> TargetDistance {
  const QPoint target = resolvePrimaryTarget(head, snapshot, config);

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

auto evaluateLeaf(const Snapshot& snapshot, const MoveState& state, const StrategyConfig& config)
  -> int {
  auto blocked = buildBlockedMap(snapshot, state.body);
  if (const auto headIndex = tryBoardIndex(state.head, snapshot.boardWidth, snapshot.boardHeight);
      headIndex.has_value()) {
    blocked[*headIndex] = false;
  }
  const int openSpace = floodReachable(state.head, snapshot, blocked);
  const int safeNeighbors = countSafeNeighbors(state.head, snapshot, blocked);
  const QPoint tailFallback = state.body.empty() ? state.head : state.body.back();
  const TargetDistance targetDistance =
    resolveTargetDistance(state.head, snapshot, config, blocked, tailFallback);
  const int trapPenalty = safeNeighbors <= 1 ? config.trapPenalty : 0;
  return (openSpace * config.openSpaceWeight) + (safeNeighbors * config.safeNeighborWeight) -
         (targetDistance.distance * config.targetDistanceWeight) - trapPenalty +
         (state.score * 48) - targetDistance.unreachablePenalty;
}

auto searchValue(const Snapshot& snapshot,
                 const MoveState& state,
                 const StrategyConfig& config,
                 const int depth) -> int {
  if (depth <= 0) {
    return evaluateLeaf(snapshot, state, config);
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
    const int score = immediate + searchValue(snapshot, preview.next, config, depth - 1);
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
                  const StrategyConfig& config) -> int {
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
      int score = evaluateLeaf(snapshot, preview.next, config);
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

auto selectLoopAwareDirection(const Snapshot& snapshot,
                              const StrategyConfig& config,
                              LoopMemory& memory,
                              const bool useSearchScoring) -> std::optional<QPoint> {
  if (snapshot.body.empty() || snapshot.boardWidth <= 0 || snapshot.boardHeight <= 0) {
    return std::nullopt;
  }
  const StrategyConfig tunedConfig = stageAdjustedStrategy(config, snapshot);
  const MoveState initial{
    .head = snapshot.head,
    .direction = snapshot.direction,
    .body = snapshot.body,
    .score = snapshot.score,
  };

  constexpr int kLoopRepeatThreshold = 4;
  constexpr int kNoScoreEscapeTicks = 28;
  const int repeats = memory.observe(snapshot, initial);
  const bool stagnantEscape = memory.noScoreTicks() >= kNoScoreEscapeTicks && repeats >= 2;
  const bool escapeMode = repeats >= kLoopRepeatThreshold || stagnantEscape;
  const int riskBudget = riskBudgetFor(snapshot, repeats);
  const int depth = std::clamp(tunedConfig.lookaheadDepth + 1, 2, 6);
  const QPoint primaryTarget = resolvePrimaryTarget(initial.head, snapshot, tunedConfig);
  const auto tieRotateSeed = static_cast<std::uint64_t>(stateHash(snapshot, initial)) ^
                             static_cast<std::uint64_t>(tunedConfig.tieBreakSeed) ^
                             memory.observeTick();
  const int tieRotateOffset =
    static_cast<int>(tieRotateSeed % static_cast<std::uint64_t>(kDirections.size()));

  int bestScore = std::numeric_limits<int>::min();
  int bestTieRank = std::numeric_limits<int>::max();
  std::optional<QPoint> bestDirection;
  for (const QPoint& candidate : kDirections) {
    const auto preview = previewMove(snapshot, initial, candidate);
    if (!preview.valid) {
      continue;
    }

    const int revisitCount = memory.repeatsFor(snapshot, preview.next);
    auto blocked = buildBlockedMap(snapshot, preview.next.body);
    if (const auto headIndex =
          tryBoardIndex(preview.next.head, snapshot.boardWidth, snapshot.boardHeight);
        headIndex.has_value()) {
      blocked[*headIndex] = false;
    }
    const int openSpace = floodReachable(preview.next.head, snapshot, blocked);
    const int safeNeighbors = countSafeNeighbors(preview.next.head, snapshot, blocked);
    const QPoint target = resolvePrimaryTarget(preview.next.head, snapshot, tunedConfig);
    const int pocketPenalty =
      pocketPenaltyTowardTarget(preview.next.head, target, snapshot, blocked);
    int score = 0;
    if (escapeMode) {
      score = evaluateEscapeCandidate(snapshot, preview, tunedConfig, revisitCount);
      score += approachTargetBonus(
        initial.head, preview.next.head, primaryTarget, snapshot, tunedConfig, repeats);
    } else if (useSearchScoring) {
      int immediate = (candidate == snapshot.direction ? tunedConfig.straightBonus : 0);
      if (preview.ateFood) {
        immediate += tunedConfig.foodConsumeBonus;
      }
      if (preview.atePower) {
        immediate += powerPriority(tunedConfig, snapshot.powerUpType);
      }
      score = immediate + searchValue(snapshot, preview.next, tunedConfig, depth - 1) -
              (revisitCount * std::max(1, tunedConfig.loopRepeatPenalty - 8));
      score += rolloutScore(snapshot, preview.next, tunedConfig) / 6;
      score += approachTargetBonus(
        initial.head, preview.next.head, primaryTarget, snapshot, tunedConfig, repeats);
      score -= pocketPenalty * 2;
    } else {
      const QPoint tailFallback =
        preview.next.body.empty() ? preview.next.head : preview.next.body.back();
      const TargetDistance targetDistance =
        resolveTargetDistance(preview.next.head, snapshot, tunedConfig, blocked, tailFallback);
      int immediate = (candidate == snapshot.direction ? tunedConfig.straightBonus : 0);
      if (preview.ateFood) {
        immediate += tunedConfig.foodConsumeBonus;
      }
      if (preview.atePower) {
        immediate += powerPriority(tunedConfig, snapshot.powerUpType);
      }
      const int trapPenalty = safeNeighbors <= 1 ? tunedConfig.trapPenalty : 0;
      score = (openSpace * tunedConfig.openSpaceWeight) +
              (safeNeighbors * tunedConfig.safeNeighborWeight) -
              (targetDistance.distance * tunedConfig.targetDistanceWeight) + immediate -
              trapPenalty - (revisitCount * tunedConfig.loopRepeatPenalty) -
              targetDistance.unreachablePenalty;
      score += approachTargetBonus(
        initial.head, preview.next.head, primaryTarget, snapshot, tunedConfig, repeats);
      score -= pocketPenalty;
    }

    const int riskCost = candidateRiskCost(openSpace, safeNeighbors, revisitCount, snapshot);
    if (riskCost > riskBudget) {
      score -= (riskCost - riskBudget) * 6;
    }

    const int rawIndex = directionIndex(candidate);
    const int tieRank = (rawIndex - tieRotateOffset + static_cast<int>(kDirections.size())) %
                        static_cast<int>(kDirections.size());
    if (score > bestScore || (score == bestScore && tieRank < bestTieRank)) {
      bestScore = score;
      bestTieRank = tieRank;
      bestDirection = candidate;
    }
  }
  return bestDirection;
}

class RuleBackend final : public BotBackend {
public:
  [[nodiscard]] auto name() const -> QString override {
    return QStringLiteral("rule");
  }

  [[nodiscard]] auto decideDirection(const Snapshot& snapshot, const StrategyConfig& config) const
    -> std::optional<QPoint> override {
    return selectLoopAwareDirection(snapshot, config, m_loopMemory, false);
  }

  [[nodiscard]] auto decideChoice(const QVariantList& choices, const StrategyConfig& config) const
    -> int override {
    return pickChoiceIndex(choices, config);
  }

  void reset() override {
    m_loopMemory.clear();
  }

private:
  mutable LoopMemory m_loopMemory;
};

class SearchBackend final : public BotBackend {
public:
  [[nodiscard]] auto name() const -> QString override {
    return QStringLiteral("search");
  }

  [[nodiscard]] auto decideDirection(const Snapshot& snapshot, const StrategyConfig& config) const
    -> std::optional<QPoint> override {
    return selectLoopAwareDirection(snapshot, config, m_loopMemory, true);
  }

  [[nodiscard]] auto decideChoice(const QVariantList& choices, const StrategyConfig& config) const
    -> int override {
    return pickChoiceIndex(choices, config);
  }

  void reset() override {
    m_loopMemory.clear();
  }

private:
  mutable LoopMemory m_loopMemory;
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
