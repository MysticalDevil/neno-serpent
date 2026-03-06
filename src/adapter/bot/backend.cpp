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

auto boardCenter(const Snapshot& snapshot) -> QPoint {
  return QPoint(snapshot.boardWidth / 2, snapshot.boardHeight / 2);
}

auto centerBandMargin(const int size) -> int {
  return std::max(2, size / 4);
}

auto isPointInCenterBand(const QPoint& point, const Snapshot& snapshot) -> bool {
  if (snapshot.boardWidth <= 0 || snapshot.boardHeight <= 0) {
    return false;
  }
  const int marginX = centerBandMargin(snapshot.boardWidth);
  const int marginY = centerBandMargin(snapshot.boardHeight);
  const int minX = marginX;
  const int maxX = std::max(minX, snapshot.boardWidth - 1 - marginX);
  const int minY = marginY;
  const int maxY = std::max(minY, snapshot.boardHeight - 1 - marginY);
  return point.x() >= minX && point.x() <= maxX && point.y() >= minY && point.y() <= maxY;
}

auto cornerDistance(const QPoint& point, const Snapshot& snapshot) -> int {
  if (snapshot.boardWidth <= 0 || snapshot.boardHeight <= 0) {
    return 0;
  }
  const std::array<QPoint, 4> corners = {
    QPoint(0, 0),
    QPoint(snapshot.boardWidth - 1, 0),
    QPoint(snapshot.boardWidth - 1, snapshot.boardHeight - 1),
    QPoint(0, snapshot.boardHeight - 1),
  };
  int best = std::numeric_limits<int>::max();
  for (const QPoint& corner : corners) {
    best =
      std::min(best, toroidalDistance(point, corner, snapshot.boardWidth, snapshot.boardHeight));
  }
  return best;
}

auto isNearCorner(const QPoint& point, const Snapshot& snapshot) -> bool {
  return cornerDistance(point, snapshot) <= 3;
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
    adjusted.modeWeights.safeNeighborWeight += 8;
    adjusted.modeWeights.openSpaceWeight += 2;
    adjusted.modeWeights.trapPenalty += 20;
    adjusted.modeWeights.targetDistanceWeight =
      std::max(0, adjusted.modeWeights.targetDistanceWeight - 2);
    adjusted.modeWeights.foodConsumeBonus = std::max(0, adjusted.modeWeights.foodConsumeBonus - 4);
    adjusted.modeWeights.straightBonus = std::max(0, adjusted.modeWeights.straightBonus - 2);
    adjusted.modeWeights.lookaheadDepth += 1;
    adjusted.modeWeights.powerTargetDistanceSlack =
      std::max(0, adjusted.modeWeights.powerTargetDistanceSlack - 1);
  } else if (policy == DecisionPolicy::Aggressive) {
    adjusted.modeWeights.safeNeighborWeight =
      std::max(0, adjusted.modeWeights.safeNeighborWeight - 6);
    adjusted.modeWeights.openSpaceWeight = std::max(0, adjusted.modeWeights.openSpaceWeight - 1);
    adjusted.modeWeights.trapPenalty = std::max(0, adjusted.modeWeights.trapPenalty - 14);
    adjusted.modeWeights.targetDistanceWeight += 2;
    adjusted.modeWeights.foodConsumeBonus += 8;
    adjusted.modeWeights.straightBonus += 4;
    adjusted.modeWeights.powerTargetDistanceSlack += 2;
  }

  if (stage.lateGame) {
    adjusted.modeWeights.safeNeighborWeight += 7;
    adjusted.modeWeights.openSpaceWeight += 3;
    adjusted.modeWeights.trapPenalty += 18;
    adjusted.modeWeights.targetDistanceWeight =
      std::max(0, adjusted.modeWeights.targetDistanceWeight - 2);
    adjusted.modeWeights.powerTargetDistanceSlack =
      std::max(1, adjusted.modeWeights.powerTargetDistanceSlack - 1);
    adjusted.modeWeights.lookaheadDepth = std::max(adjusted.modeWeights.lookaheadDepth, 3);
  }

  if (stage.crowded) {
    adjusted.modeWeights.safeNeighborWeight += 4;
    adjusted.modeWeights.trapPenalty += 14;
    adjusted.modeWeights.targetDistanceWeight =
      std::max(0, adjusted.modeWeights.targetDistanceWeight - 1);
    adjusted.modeWeights.lookaheadDepth = std::max(adjusted.modeWeights.lookaheadDepth, 3);
  }

  if (snapshot.score < 20 && stage.snakeFillPermille < 140 && !stage.crowded) {
    adjusted.modeWeights.foodConsumeBonus += 4;
    adjusted.modeWeights.straightBonus += 2;
    if (adjusted.modeWeights.targetDistanceWeight > 0) {
      adjusted.modeWeights.targetDistanceWeight += 1;
    }
  }

  adjusted.modeWeights.openSpaceWeight = clampInt(adjusted.modeWeights.openSpaceWeight, 0, 60);
  adjusted.modeWeights.safeNeighborWeight =
    clampInt(adjusted.modeWeights.safeNeighborWeight, 0, 90);
  adjusted.modeWeights.targetDistanceWeight =
    clampInt(adjusted.modeWeights.targetDistanceWeight, 0, 80);
  adjusted.modeWeights.foodConsumeBonus = clampInt(adjusted.modeWeights.foodConsumeBonus, 0, 100);
  adjusted.modeWeights.trapPenalty = clampInt(adjusted.modeWeights.trapPenalty, 0, 160);
  adjusted.modeWeights.lookaheadDepth = clampInt(adjusted.modeWeights.lookaheadDepth, 0, 6);
  adjusted.modeWeights.powerTargetDistanceSlack =
    clampInt(adjusted.modeWeights.powerTargetDistanceSlack, 0, 20);
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

auto directionIndex(const QPoint& direction) -> int;

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
    m_recentDirections.clear();
    m_tabooActionTicks.fill(0);
    m_cycle4Count = 0;
    m_cycle6Count = 0;
    m_cycle8Count = 0;
    m_tabooHits = 0;
    m_cycleBurstScore = 0;
    m_cycleTrendTicks = 0;
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

  [[nodiscard]] auto cycle4Count() const -> int {
    return m_cycle4Count;
  }

  [[nodiscard]] auto cycle6Count() const -> int {
    return m_cycle6Count;
  }

  [[nodiscard]] auto cycle8Count() const -> int {
    return m_cycle8Count;
  }

  [[nodiscard]] auto tabooHits() const -> int {
    return m_tabooHits;
  }

  [[nodiscard]] auto centerRecoverRecommended() const -> bool {
    return m_cycleBurstScore >= 3 || m_cycleTrendTicks >= 2;
  }

  [[nodiscard]] auto recoveryTarget(const Snapshot& snapshot) const -> QPoint {
    const QPoint center = boardCenter(snapshot);
    auto axisTarget = [](const int headCoord, const int centerCoord, const int boardSize) -> int {
      const int delta = centerCoord - headCoord;
      if (std::abs(delta) <= 2) {
        return centerCoord;
      }
      const int step = std::clamp(delta / 2, -4, 4);
      const int margin = std::min(2, std::max(0, (boardSize / 2) - 1));
      return std::clamp(headCoord + step, margin, std::max(margin, boardSize - margin - 1));
    };
    const QPoint waypoint(axisTarget(snapshot.head.x(), center.x(), snapshot.boardWidth),
                          axisTarget(snapshot.head.y(), center.y(), snapshot.boardHeight));
    return waypoint == snapshot.head ? center : waypoint;
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
    const auto& guard = config.loopGuard;
    if (escapeMode) {
      return clampInt(revisitCount * guard.loopEscapePenalty, 0, 520);
    }
    const int revisitPenalty =
      revisitCount *
      (useSearchScoring ? std::max(1, guard.loopRepeatPenalty - 8) : guard.loopRepeatPenalty);
    const int pocketScale = useSearchScoring ? 2 : 1;
    return clampInt(revisitPenalty + (pocketPenalty * pocketScale), 0, 420);
  }

  [[nodiscard]] auto actionSequencePenalty(const QPoint& candidate,
                                           const bool escapeMode,
                                           const int noScoreTicks,
                                           const StrategyConfig& config) const -> int {
    const auto& guard = config.loopGuard;
    const auto periodicRepeat = [&](const int period) -> bool {
      const int n = static_cast<int>(m_recentDirections.size());
      if (n < (period * 2) - 1) {
        return false;
      }
      for (int i = 0; i < period; ++i) {
        const QPoint lhs = (i == period - 1)
                             ? candidate
                             : m_recentDirections[static_cast<std::size_t>(n - period + i)];
        const QPoint rhs = m_recentDirections[static_cast<std::size_t>(n - (period * 2) + i + 1)];
        if (lhs != rhs) {
          return false;
        }
      }
      return true;
    };

    int repeatPenalty = 0;
    int streak = 0;
    for (auto it = m_recentDirections.rbegin(); it != m_recentDirections.rend(); ++it) {
      if (*it != candidate) {
        break;
      }
      ++streak;
    }
    if (escapeMode) {
      if (streak >= 2) {
        repeatPenalty =
          (streak - 1) * (guard.repeatStreakPenalty + std::max(0, (noScoreTicks - 24) / 7));
      }
    } else if (streak >= 3) {
      repeatPenalty =
        (streak - 2) * ((guard.repeatStreakPenalty * 3 / 5) + std::max(0, (noScoreTicks - 12) / 9));
    }

    int cyclePenalty = 0;
    if (periodicRepeat(4)) {
      cyclePenalty += guard.cycle4Penalty + std::max(0, noScoreTicks - 16) / 2;
    }
    if (periodicRepeat(6)) {
      cyclePenalty += guard.cycle6Penalty + std::max(0, noScoreTicks - 20) / 2;
    }
    if (periodicRepeat(8)) {
      cyclePenalty += guard.cycle8Penalty + std::max(0, noScoreTicks - 24);
    }
    if (!escapeMode) {
      cyclePenalty = (cyclePenalty * 3) / 4;
    }

    int tabooPenalty = 0;
    const int candidateIndex = directionIndex(candidate);
    if (candidateIndex >= 0 && m_tabooActionTicks[static_cast<std::size_t>(candidateIndex)] > 0) {
      tabooPenalty = (escapeMode ? guard.tabooPenalty : (guard.tabooPenalty * 2 / 3)) +
                     std::max(0, noScoreTicks - 20) / 2;
    }
    return repeatPenalty + cyclePenalty + tabooPenalty;
  }

  auto observeDecision(const std::optional<QPoint>& direction,
                       const bool escapeMode,
                       const StrategyConfig& config) -> void {
    if (!direction.has_value()) {
      return;
    }
    for (int& tabooTicks : m_tabooActionTicks) {
      if (tabooTicks > 0) {
        --tabooTicks;
      }
    }
    m_recentDirections.push_back(*direction);
    while (m_recentDirections.size() > 20) {
      m_recentDirections.pop_front();
    }
    const auto recentCycleDetected = [&](const int period) -> bool {
      const int n = static_cast<int>(m_recentDirections.size());
      if (n < period * 2) {
        return false;
      }
      for (int i = 0; i < period; ++i) {
        const QPoint lhs = m_recentDirections[static_cast<std::size_t>(n - period + i)];
        const QPoint rhs = m_recentDirections[static_cast<std::size_t>(n - (period * 2) + i)];
        if (lhs != rhs) {
          return false;
        }
      }
      return true;
    };
    const bool cycle4 = recentCycleDetected(4);
    const bool cycle6 = recentCycleDetected(6);
    const bool cycle8 = recentCycleDetected(8);
    if (cycle4) {
      ++m_cycle4Count;
    }
    if (cycle6) {
      ++m_cycle6Count;
    }
    if (cycle8) {
      ++m_cycle8Count;
    }
    if (cycle4 || cycle6 || cycle8) {
      const int tabooTicks =
        escapeMode ? config.loopGuard.tabooEscapeTicks : config.loopGuard.tabooNormalTicks;
      applyTaboo(*direction, tabooTicks);
      if (m_recentDirections.size() >= 2) {
        applyTaboo(m_recentDirections[m_recentDirections.size() - 2], std::max(1, tabooTicks - 1));
      }
      ++m_tabooHits;
      m_cycleBurstScore += cycle8 ? 4 : (cycle6 ? 3 : 2);
      m_cycleBurstScore = std::min(m_cycleBurstScore, 12);
      m_cycleTrendTicks += cycle6 || cycle8 ? 2 : 1;
      m_cycleTrendTicks = std::min(m_cycleTrendTicks, 8);
    } else if (m_cycleBurstScore > 0) {
      --m_cycleBurstScore;
      m_cycleTrendTicks = std::max(0, m_cycleTrendTicks - 1);
    }
    if (escapeMode) {
      m_escapeHistory.push_back(*direction);
      while (m_escapeHistory.size() > 12) {
        m_escapeHistory.pop_front();
      }
    }
  }

private:
  auto applyTaboo(const QPoint& direction, const int ticks) -> void {
    const int index = directionIndex(direction);
    if (index < 0) {
      return;
    }
    auto& slot = m_tabooActionTicks[static_cast<std::size_t>(index)];
    slot = std::max(slot, ticks);
  }

  int m_lastScore = 0;
  int m_noScoreTicks = 0;
  bool m_hasScore = false;
  std::deque<QPoint> m_escapeHistory;
  std::deque<QPoint> m_recentDirections;
  std::array<int, 4> m_tabooActionTicks = {0, 0, 0, 0};
  int m_cycle4Count = 0;
  int m_cycle6Count = 0;
  int m_cycle8Count = 0;
  int m_tabooHits = 0;
  int m_cycleBurstScore = 0;
  int m_cycleTrendTicks = 0;
};

enum class TargetMode {
  FoodChase,
  PowerChase,
  Escape,
  CenterRecover,
};

class ModePlanner {
public:
  auto clear() -> void {
    m_mode = TargetMode::FoodChase;
    m_modeTicks = 0;
    m_lastObserveTick = 0;
    m_forceTailChaseTicks = 0;
    m_powerChaseCooldownTicks = 0;
    m_centerRecoverTicks = 0;
    m_escapeWindow.clear();
  }

  auto mode() const -> TargetMode {
    return m_mode;
  }

  [[nodiscard]] auto suppressPowerChase() const -> bool {
    return m_powerChaseCooldownTicks > 0 || m_forceTailChaseTicks > 0 || m_centerRecoverTicks > 0;
  }

  [[nodiscard]] auto forceTailChaseActive() const -> bool {
    return m_forceTailChaseTicks > 0;
  }

  auto update(const Snapshot& snapshot,
              const StrategyConfig& config,
              const LoopController& loopController,
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
      if (m_centerRecoverTicks > 0) {
        --m_centerRecoverTicks;
      }
    }

    if (noScoreTicks >= 56) {
      m_forceTailChaseTicks = std::max(m_forceTailChaseTicks, 24);
      m_powerChaseCooldownTicks = std::max(m_powerChaseCooldownTicks, 40);
    }

    const int centerDistance = toroidalDistance(
      snapshot.head, boardCenter(snapshot), snapshot.boardWidth, snapshot.boardHeight);
    if (loopController.centerRecoverRecommended() && centerDistance >= 2) {
      const int recoverTicks =
        config.recovery.centerRecoverTicks + std::max(0, noScoreTicks - 24) / 4;
      m_centerRecoverTicks = std::max(m_centerRecoverTicks, recoverTicks);
    }
    if (m_centerRecoverTicks > 0) {
      if (m_mode != TargetMode::CenterRecover) {
        switchMode(TargetMode::CenterRecover);
      }
      recordEscapeMode();
      return;
    }

    const bool hasPowerCandidate = snapshot.powerUpPos.x() >= 0 && snapshot.powerUpPos.y() >= 0 &&
                                   powerPriority(config, snapshot.powerUpType) >=
                                     config.modeWeights.powerTargetPriorityThreshold;
    const bool hasPower = hasPowerCandidate && !suppressPowerChase();
    const bool hardEscape = noScoreTicks >= 96 || repeats >= 6;
    bool wantEscape = repeats >= 4 || noScoreTicks >= 28 || m_forceTailChaseTicks > 0;
    if (wantEscape && !hardEscape &&
        escapeRatioPermille() >= config.recovery.escapeRatioSoftCapPermille && noScoreTicks < 72) {
      wantEscape = false;
    }
    const TargetMode desired =
      wantEscape ? TargetMode::Escape : (hasPower ? TargetMode::PowerChase : TargetMode::FoodChase);

    if (desired == m_mode) {
      recordEscapeMode();
      return;
    }
    if (desired == TargetMode::Escape) {
      switchMode(desired);
      recordEscapeMode();
      return;
    }
    if (m_mode == TargetMode::Escape) {
      if (m_modeTicks >= 6) {
        switchMode(desired);
      }
      recordEscapeMode();
      return;
    }
    if (m_modeTicks >= 10) {
      switchMode(desired);
    }
    recordEscapeMode();
  }

  auto targetPoint(const Snapshot& snapshot,
                   const StrategyConfig& config,
                   const LoopController& loopController) const -> QPoint {
    if (m_forceTailChaseTicks > 0) {
      return snapshot.body.empty() ? snapshot.food : snapshot.body.back();
    }
    if (m_mode == TargetMode::CenterRecover) {
      return loopController.recoveryTarget(snapshot);
    }
    if (m_mode == TargetMode::Escape) {
      return snapshot.body.empty() ? snapshot.food : snapshot.body.back();
    }
    if (m_mode == TargetMode::PowerChase && snapshot.powerUpPos.x() >= 0 &&
        snapshot.powerUpPos.y() >= 0 &&
        powerPriority(config, snapshot.powerUpType) >=
          config.modeWeights.powerTargetPriorityThreshold) {
      return snapshot.powerUpPos;
    }
    return snapshot.food;
  }

private:
  auto recordEscapeMode() -> void {
    m_escapeWindow.push_back(m_mode == TargetMode::Escape);
    while (m_escapeWindow.size() > 96) {
      m_escapeWindow.pop_front();
    }
  }

  [[nodiscard]] auto escapeRatioPermille() const -> int {
    if (m_escapeWindow.empty()) {
      return 0;
    }
    int escapeTicks = 0;
    for (const bool active : m_escapeWindow) {
      if (active) {
        ++escapeTicks;
      }
    }
    return (escapeTicks * 1000) / static_cast<int>(m_escapeWindow.size());
  }

  auto switchMode(const TargetMode mode) -> void {
    m_mode = mode;
    m_modeTicks = 0;
  }

  TargetMode m_mode = TargetMode::FoodChase;
  int m_modeTicks = 0;
  int m_forceTailChaseTicks = 0;
  int m_powerChaseCooldownTicks = 0;
  int m_centerRecoverTicks = 0;
  std::uint64_t m_lastObserveTick = 0;
  std::deque<bool> m_escapeWindow;
};

auto targetModeName(const TargetMode mode) -> QString {
  switch (mode) {
  case TargetMode::FoodChase:
    return QStringLiteral("FoodChase");
  case TargetMode::PowerChase:
    return QStringLiteral("PowerChase");
  case TargetMode::Escape:
    return QStringLiteral("Escape");
  case TargetMode::CenterRecover:
    return QStringLiteral("CenterRecover");
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
  const int trapPenalty = safeNeighbors <= 1 ? config.modeWeights.trapPenalty : 0;
  return (openSpace * config.modeWeights.openSpaceWeight) +
         (safeNeighbors * config.modeWeights.safeNeighborWeight) -
         (targetDistance.distance * config.modeWeights.targetDistanceWeight) - trapPenalty +
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
    int immediate = (candidate == state.direction ? config.modeWeights.straightBonus : 0);
    if (preview.ateFood) {
      immediate += config.modeWeights.foodConsumeBonus;
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
  return clampInt(8 + (config.modeWeights.lookaheadDepth * 2), 8, 20);
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
        score += config.modeWeights.foodConsumeBonus * 2;
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

  int score = (openSpace * config.modeWeights.openSpaceWeight * 2) +
              (safeNeighbors * config.modeWeights.safeNeighborWeight * 3) + (tailDistance * 14) -
              (revisitCount * config.loopGuard.loopEscapePenalty);
  if (preview.ateFood) {
    score += config.modeWeights.foodConsumeBonus * 6;
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
  int scale = config.modeWeights.targetDistanceWeight + (config.modeWeights.foodConsumeBonus / 2);
  if (target == snapshot.food && snapshot.score < 60 &&
      static_cast<int>(snapshot.body.size()) < 12) {
    scale += config.modeWeights.foodConsumeBonus / 2;
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

struct DecisionContext {
  const Snapshot& snapshot;
  const StrategyConfig& config;
  const MoveState& initial;
  const LoopController& loopController;
  const ModePlanner& modePlanner;
  const QPoint& primaryTarget;
  const QPoint& boardMid;
  bool useSearchScoring = false;
  bool escapeMode = false;
  int noScoreTicks = 0;
  int repeats = 0;
  int riskBudget = 0;
  int depth = 0;
  int currentPrimaryDistance = 0;
  int currentFoodDistance = 0;
  bool centerFoodPush = false;
  bool earlyFoodChaseGuard = false;
  bool hasNonWorseningFoodMove = false;
  bool deepEscapeStall = false;
  std::uint64_t tieRotateSeed = 0;
  int tieRotateOffset = 0;
  int orbitBreakLevel = 0;
  int orbitPreferredIndex = -1;
};

struct CandidateEvaluation {
  ScoreBreakdown breakdown;
  int score = std::numeric_limits<int>::min();
  int tieRank = std::numeric_limits<int>::max();
};

auto evaluateCandidateScore(const CandidateStats& candidateStats, const DecisionContext& ctx)
  -> CandidateEvaluation {
  CandidateEvaluation evaluation{};
  const QPoint candidate = candidateStats.candidate;
  const int candidateIndex = directionIndex(candidate);
  const MovePreview& preview = candidateStats.preview;
  const int revisitCount = candidateStats.revisitCount;
  const int openSpace = candidateStats.openSpace;
  const int safeNeighbors = candidateStats.safeNeighbors;
  const auto& blocked = candidateStats.blocked;
  const int pocketPenalty =
    pocketPenaltyTowardTarget(preview.next.head, ctx.primaryTarget, ctx.snapshot, blocked);
  const int boardArea = std::max(1, ctx.snapshot.boardWidth * ctx.snapshot.boardHeight);
  const int openSpacePct = (openSpace * 100) / boardArea;
  const int normalizedSafeNeighbors = safeNeighbors * 20;

  if (ctx.escapeMode) {
    const int escapeBase = evaluateEscapeCandidate(ctx.snapshot, preview, ctx.config, revisitCount);
    const int compressedEscapeBase = (escapeBase * 3) / 10;
    const int openSpaceTerm = (openSpacePct * 7) / 4;
    const int safeNeighborTerm = safeNeighbors * 22;
    const int tailReachTerm = candidateStats.tailReachable ? 14 : -42;
    const int stallDecay = std::min(120, std::max(0, ctx.noScoreTicks - 20) * 3);
    const int escapeSurvival =
      compressedEscapeBase + openSpaceTerm + safeNeighborTerm + tailReachTerm - stallDecay;
    evaluation.breakdown.survival = clampScoreBlock(escapeSurvival, -60, 190);
    evaluation.breakdown.progress = clampScoreBlock(approachTargetBonus(ctx.initial.head,
                                                                        preview.next.head,
                                                                        ctx.primaryTarget,
                                                                        ctx.snapshot,
                                                                        ctx.config,
                                                                        ctx.repeats),
                                                    -80,
                                                    120);
    if (preview.ateFood) {
      evaluation.breakdown.reward += ctx.config.modeWeights.foodConsumeBonus * 2;
    }
    if (preview.atePower && !ctx.modePlanner.suppressPowerChase()) {
      evaluation.breakdown.reward += powerPriority(ctx.config, ctx.snapshot.powerUpType) * 2;
    }
    evaluation.breakdown.reward = clampScoreBlock(evaluation.breakdown.reward, 0, 240);
    if (ctx.orbitBreakLevel > 0) {
      if (ctx.orbitPreferredIndex >= 0 && candidate == kDirections[ctx.orbitPreferredIndex]) {
        evaluation.breakdown.progress += ctx.orbitBreakLevel * 10;
      }
      if (candidate == ctx.snapshot.direction) {
        evaluation.breakdown.progress -= ctx.orbitBreakLevel * 8;
      }
      const int straightIndex = directionIndex(ctx.snapshot.direction);
      if (straightIndex >= 0 &&
          candidateIndex == ((straightIndex + 2) % static_cast<int>(kDirections.size()))) {
        evaluation.breakdown.progress -= ctx.orbitBreakLevel * 6;
      }
      evaluation.breakdown.progress = clampScoreBlock(evaluation.breakdown.progress, -120, 170);
    }
    const int repeatSquaredPenalty = std::min(260, revisitCount * revisitCount * 12);
    const int escapeLoopExtra =
      std::min(320,
               (revisitCount * 24) + repeatSquaredPenalty + (pocketPenalty * 10) +
                 (ctx.orbitBreakLevel * 18) + std::max(0, ctx.noScoreTicks - 24));
    evaluation.breakdown.loopCost =
      clampScoreBlock(ctx.loopController.loopCost(
                        revisitCount, pocketPenalty, ctx.config, ctx.useSearchScoring, true) +
                        escapeLoopExtra,
                      0,
                      520);
  } else if (ctx.useSearchScoring) {
    int immediate =
      (candidate == ctx.snapshot.direction ? ctx.config.modeWeights.straightBonus : 0);
    if (preview.ateFood) {
      immediate += ctx.config.modeWeights.foodConsumeBonus;
    }
    if (preview.atePower && !ctx.modePlanner.suppressPowerChase()) {
      immediate += powerPriority(ctx.config, ctx.snapshot.powerUpType);
    }
    const QPoint tailFallback =
      preview.next.body.empty() ? preview.next.head : preview.next.body.back();
    const TargetDistance targetDistance = resolveTargetDistance(
      preview.next.head, ctx.primaryTarget, ctx.snapshot, blocked, tailFallback);
    const int searchTerm =
      searchValue(ctx.snapshot, preview.next, ctx.config, ctx.depth - 1, ctx.primaryTarget);
    const int rolloutTerm =
      rolloutScore(ctx.snapshot, preview.next, ctx.config, ctx.primaryTarget) / 6;
    evaluation.breakdown.progress =
      clampScoreBlock(approachTargetBonus(ctx.initial.head,
                                          preview.next.head,
                                          ctx.primaryTarget,
                                          ctx.snapshot,
                                          ctx.config,
                                          ctx.repeats) +
                        (searchTerm / 8) + (rolloutTerm / 4) -
                        (targetDistance.distance * ctx.config.modeWeights.targetDistanceWeight) -
                        targetDistance.unreachablePenalty,
                      -160,
                      190);
    evaluation.breakdown.survival = clampScoreBlock(
      ((openSpacePct * ctx.config.modeWeights.openSpaceWeight) / 8) + normalizedSafeNeighbors +
        (safeNeighbors * ctx.config.modeWeights.safeNeighborWeight) +
        (candidateStats.tailReachable ? 22 : -44),
      -120,
      170);
    evaluation.breakdown.reward = clampScoreBlock(immediate, 0, 220);
    evaluation.breakdown.loopCost =
      ctx.loopController.loopCost(revisitCount, pocketPenalty, ctx.config, true, false);
  } else {
    const QPoint tailFallback =
      preview.next.body.empty() ? preview.next.head : preview.next.body.back();
    const TargetDistance targetDistance = resolveTargetDistance(
      preview.next.head, ctx.primaryTarget, ctx.snapshot, blocked, tailFallback);
    int immediate =
      (candidate == ctx.snapshot.direction ? ctx.config.modeWeights.straightBonus : 0);
    if (preview.ateFood) {
      immediate += ctx.config.modeWeights.foodConsumeBonus;
    }
    if (preview.atePower && !ctx.modePlanner.suppressPowerChase()) {
      immediate += powerPriority(ctx.config, ctx.snapshot.powerUpType);
    }
    evaluation.breakdown.progress =
      clampScoreBlock(approachTargetBonus(ctx.initial.head,
                                          preview.next.head,
                                          ctx.primaryTarget,
                                          ctx.snapshot,
                                          ctx.config,
                                          ctx.repeats) -
                        (targetDistance.distance * ctx.config.modeWeights.targetDistanceWeight) -
                        targetDistance.unreachablePenalty,
                      -140,
                      170);
    evaluation.breakdown.survival = clampScoreBlock(
      ((openSpacePct * ctx.config.modeWeights.openSpaceWeight) / 8) + normalizedSafeNeighbors +
        (safeNeighbors * ctx.config.modeWeights.safeNeighborWeight) +
        (candidateStats.tailReachable ? 20 : -44),
      -110,
      160);
    evaluation.breakdown.reward = clampScoreBlock(immediate, 0, 220);
    evaluation.breakdown.loopCost =
      ctx.loopController.loopCost(revisitCount, pocketPenalty, ctx.config, false, false);
  }

  const int actionSequencePenalty = ctx.loopController.actionSequencePenalty(
    candidate, ctx.escapeMode, ctx.noScoreTicks, ctx.config);
  evaluation.breakdown.loopCost = clampScoreBlock(
    evaluation.breakdown.loopCost + actionSequencePenalty, 0, ctx.escapeMode ? 580 : 420);

  if (ctx.centerFoodPush) {
    const int centerDelta =
      toroidalDistance(
        ctx.initial.head, ctx.boardMid, ctx.snapshot.boardWidth, ctx.snapshot.boardHeight) -
      toroidalDistance(
        preview.next.head, ctx.boardMid, ctx.snapshot.boardWidth, ctx.snapshot.boardHeight);
    const bool leaveCorner = isNearCorner(ctx.initial.head, ctx.snapshot) &&
                             !isNearCorner(preview.next.head, ctx.snapshot);
    const bool stuckCorner = !isNearCorner(ctx.initial.head, ctx.snapshot) &&
                             isNearCorner(preview.next.head, ctx.snapshot);
    evaluation.breakdown.progress += centerDelta * ctx.config.recovery.centerBiasWeight;
    if (leaveCorner) {
      evaluation.breakdown.progress += ctx.config.recovery.cornerLeaveBonus;
    }
    if (stuckCorner) {
      evaluation.breakdown.progress -= ctx.config.recovery.cornerStickPenalty;
    }
    evaluation.breakdown.progress = clampScoreBlock(evaluation.breakdown.progress, -260, 260);
  }

  const int riskCost = candidateRiskCost(openSpace, safeNeighbors, revisitCount, ctx.snapshot);
  const int trapPenalty = safeNeighbors <= 1 ? ctx.config.modeWeights.trapPenalty : 0;
  evaluation.breakdown.risk =
    clampScoreBlock(trapPenalty + std::max(0, riskCost - ctx.riskBudget) * 6, 0, 320);
  if (ctx.snapshot.score < 50 && static_cast<int>(ctx.snapshot.body.size()) < 14) {
    if (safeNeighbors <= 2) {
      evaluation.breakdown.risk =
        clampScoreBlock(evaluation.breakdown.risk + ((3 - safeNeighbors) * 48), 0, 380);
    }
    if (openSpace < static_cast<int>(preview.next.body.size()) + 8) {
      evaluation.breakdown.risk = clampScoreBlock(evaluation.breakdown.risk + 56, 0, 380);
    }
  }

  evaluation.score = evaluation.breakdown.total();
  if (ctx.deepEscapeStall) {
    const int anchorDistance = toroidalDistance(
      preview.next.head, ctx.primaryTarget, ctx.snapshot.boardWidth, ctx.snapshot.boardHeight);
    const int escapeActionPenalty =
      ctx.loopController.actionSequencePenalty(candidate, true, ctx.noScoreTicks, ctx.config);
    const int directionalPenalty = candidate == ctx.snapshot.direction ? 50 : 0;
    const int deterministicJitter =
      static_cast<int>(((ctx.tieRotateSeed + static_cast<std::uint64_t>((candidateIndex + 1) * 17) +
                         static_cast<std::uint64_t>(ctx.noScoreTicks)) %
                        9ULL)) -
      4;
    const int deepEscapeScore =
      (openSpacePct * 4) + (safeNeighbors * 52) + (candidateStats.tailReachable ? 48 : -96) -
      (anchorDistance * 28) - (revisitCount * 64) - evaluation.breakdown.loopCost -
      evaluation.breakdown.risk - escapeActionPenalty - directionalPenalty + deterministicJitter;
    evaluation.breakdown.progress = clampScoreBlock(deepEscapeScore, -520, 320);
    evaluation.breakdown.survival = 0;
    evaluation.breakdown.reward = 0;
    evaluation.breakdown.drift = 0;
    evaluation.breakdown.risk = 0;
    evaluation.breakdown.loopCost = 0;
    evaluation.score = evaluation.breakdown.total();
  }

  if (!ctx.escapeMode && ctx.noScoreTicks >= 12) {
    const int nextPrimaryDistance = toroidalDistance(
      preview.next.head, ctx.primaryTarget, ctx.snapshot.boardWidth, ctx.snapshot.boardHeight);
    const int distanceDelta = nextPrimaryDistance - ctx.currentPrimaryDistance;
    if (distanceDelta > 0) {
      const int stallPenalty = std::min(180, distanceDelta * (8 + (ctx.noScoreTicks / 4)));
      evaluation.breakdown.drift = -stallPenalty;
    } else if (distanceDelta < 0) {
      const int stallBonus = std::min(96, (-distanceDelta) * (6 + (ctx.noScoreTicks / 8)));
      evaluation.breakdown.drift = stallBonus;
    }
    evaluation.score = evaluation.breakdown.total();
  }

  if (ctx.noScoreTicks >= ctx.config.recovery.noProgressPenaltyBase) {
    const int nextFoodDistance = toroidalDistance(
      preview.next.head, ctx.snapshot.food, ctx.snapshot.boardWidth, ctx.snapshot.boardHeight);
    const int foodDelta = nextFoodDistance - ctx.currentFoodDistance;
    if (foodDelta >= 0) {
      const int noProgressPenalty = std::min(
        420,
        (foodDelta + 1) * (ctx.config.recovery.noProgressPenaltyBase +
                           (ctx.noScoreTicks / ctx.config.recovery.noProgressPenaltyScale)));
      evaluation.breakdown.drift -= noProgressPenalty;
    } else {
      const int progressBonus = std::min(150, (-foodDelta) * (10 + (ctx.noScoreTicks / 10)));
      evaluation.breakdown.drift += progressBonus;
    }
    evaluation.score = evaluation.breakdown.total();
  }

  if (ctx.earlyFoodChaseGuard && ctx.hasNonWorseningFoodMove) {
    const int nextFoodDistance = toroidalDistance(
      preview.next.head, ctx.snapshot.food, ctx.snapshot.boardWidth, ctx.snapshot.boardHeight);
    if (nextFoodDistance > ctx.currentFoodDistance) {
      evaluation.score -= 720;
    }
  }

  const int rawIndex = candidateIndex;
  evaluation.tieRank = (rawIndex - ctx.tieRotateOffset + static_cast<int>(kDirections.size())) %
                       static_cast<int>(kDirections.size());
  if (ctx.deepEscapeStall) {
    evaluation.tieRank +=
      ctx.loopController.actionSequencePenalty(candidate, true, ctx.noScoreTicks, ctx.config) / 14;
  }
  return evaluation;
}

auto collectLegalCandidates(const Snapshot& snapshot, const MoveState& initial, LoopMemory& memory)
  -> std::vector<CandidateStats> {
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
  return legalCandidates;
}

auto formatDecisionSummary(const FilterStats& filterStats,
                           const std::vector<CandidateTelemetry>& candidateTelemetry,
                           const ModePlanner& modePlanner,
                           const LoopController& loopController,
                           const std::optional<QPoint>& bestDirection,
                           const int bestScore) -> QString {
  std::vector<CandidateTelemetry> sortedTelemetry = candidateTelemetry;
  std::sort(sortedTelemetry.begin(),
            sortedTelemetry.end(),
            [](const CandidateTelemetry& lhs, const CandidateTelemetry& rhs) {
              return lhs.total > rhs.total;
            });
  const int topCount = std::min(3, static_cast<int>(sortedTelemetry.size()));
  QStringList topItems;
  topItems.reserve(topCount);
  for (int i = 0; i < topCount; ++i) {
    const auto& item = sortedTelemetry[static_cast<std::size_t>(i)];
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
  return QStringLiteral(
           "bot decision: mode=%1 legal=%2 strict_ok=%3 reject{safe=%4 space=%5 tail=%6}"
           " viable=%7 selected=(%8,%9) score=%10 loops{c4=%11 c6=%12 c8=%13 taboo=%14}"
           " top3=%15")
    .arg(targetModeName(modePlanner.mode()))
    .arg(filterStats.legal)
    .arg(filterStats.strictAccepted)
    .arg(filterStats.strictSafeReject)
    .arg(filterStats.strictSpaceReject)
    .arg(filterStats.strictTailReject)
    .arg(candidateTelemetry.size())
    .arg(bestDirection.has_value() ? bestDirection->x() : 0)
    .arg(bestDirection.has_value() ? bestDirection->y() : 0)
    .arg(bestScore)
    .arg(loopController.cycle4Count())
    .arg(loopController.cycle6Count())
    .arg(loopController.cycle8Count())
    .arg(loopController.tabooHits())
    .arg(topItems.join(QStringLiteral(" ")));
}

auto selectLoopAwareDirection(const Snapshot& snapshot,
                              const StrategyConfig& config,
                              LoopMemory& memory,
                              LoopController& loopController,
                              ModePlanner& modePlanner,
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
  modePlanner.update(
    snapshot, tunedConfig, loopController, repeats, noScoreTicks, memory.observeTick());
  const bool escapeMode =
    (modePlanner.mode() == TargetMode::Escape) || loopController.escapeMode(repeats);
  const int riskBudget = riskBudgetFor(snapshot, repeats);
  const int depth = std::clamp(tunedConfig.modeWeights.lookaheadDepth + 1, 2, 6);
  QPoint primaryTarget = modePlanner.targetPoint(snapshot, tunedConfig, loopController);
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
  const int currentFoodDistance =
    toroidalDistance(initial.head, snapshot.food, snapshot.boardWidth, snapshot.boardHeight);
  auto initialBlocked = buildBlockedMap(snapshot, initial.body);
  if (const auto headIndex = tryBoardIndex(initial.head, snapshot.boardWidth, snapshot.boardHeight);
      headIndex.has_value()) {
    initialBlocked[*headIndex] = false;
  }
  const bool foodReachable =
    shortestReachableDistance(initial.head, snapshot.food, snapshot, initialBlocked).has_value();
  const bool centerFoodPush = foodReachable && isPointInCenterBand(snapshot.food, snapshot);
  const QPoint boardMid = boardCenter(snapshot);
  const bool earlyFoodChaseGuard = (modePlanner.mode() == TargetMode::FoodChase) &&
                                   (primaryTarget == snapshot.food) && snapshot.score < 40 &&
                                   static_cast<int>(snapshot.body.size()) < 12 && !escapeMode;

  std::vector<CandidateStats> legalCandidates = collectLegalCandidates(snapshot, initial, memory);
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
  const DecisionContext decisionContext{
    .snapshot = snapshot,
    .config = tunedConfig,
    .initial = initial,
    .loopController = loopController,
    .modePlanner = modePlanner,
    .primaryTarget = primaryTarget,
    .boardMid = boardMid,
    .useSearchScoring = useSearchScoring,
    .escapeMode = escapeMode,
    .noScoreTicks = noScoreTicks,
    .repeats = repeats,
    .riskBudget = riskBudget,
    .depth = depth,
    .currentPrimaryDistance = currentPrimaryDistance,
    .currentFoodDistance = currentFoodDistance,
    .centerFoodPush = centerFoodPush,
    .earlyFoodChaseGuard = earlyFoodChaseGuard,
    .hasNonWorseningFoodMove = hasNonWorseningFoodMove,
    .deepEscapeStall = deepEscapeStall,
    .tieRotateSeed = tieRotateSeed,
    .tieRotateOffset = tieRotateOffset,
    .orbitBreakLevel = orbitBreakLevel,
    .orbitPreferredIndex = orbitPreferredIndex,
  };
  for (const CandidateStats* candidateStats : viable) {
    const CandidateEvaluation evaluation = evaluateCandidateScore(*candidateStats, decisionContext);
    if (evaluation.score > bestScore ||
        (evaluation.score == bestScore && evaluation.tieRank < bestTieRank)) {
      bestScore = evaluation.score;
      bestTieRank = evaluation.tieRank;
      bestDirection = candidateStats->candidate;
    }
    candidateTelemetry.push_back({.direction = candidateStats->candidate,
                                  .breakdown = evaluation.breakdown,
                                  .total = evaluation.score});
  }
  if (decisionSummaryOut != nullptr) {
    *decisionSummaryOut = formatDecisionSummary(
      filterStats, candidateTelemetry, modePlanner, loopController, bestDirection, bestScore);
  }
  loopController.observeDecision(bestDirection, escapeMode, tunedConfig);
  return bestDirection;
}

class RuleBackend final : public BotBackend {
public:
  [[nodiscard]] auto name() const -> QString override {
    return QStringLiteral("rule");
  }

  [[nodiscard]] auto decideDirection(const Snapshot& snapshot, const StrategyConfig& config) const
    -> std::optional<QPoint> override {
    return selectLoopAwareDirection(snapshot,
                                    config,
                                    m_loopMemory,
                                    m_loopController,
                                    m_modePlanner,
                                    &m_lastDecisionSummary,
                                    false);
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
    m_modePlanner.clear();
    m_lastDecisionSummary.clear();
  }

private:
  mutable LoopMemory m_loopMemory;
  mutable LoopController m_loopController;
  mutable ModePlanner m_modePlanner;
  mutable QString m_lastDecisionSummary;
};

class SearchBackend final : public BotBackend {
public:
  [[nodiscard]] auto name() const -> QString override {
    return QStringLiteral("search");
  }

  [[nodiscard]] auto decideDirection(const Snapshot& snapshot, const StrategyConfig& config) const
    -> std::optional<QPoint> override {
    return selectLoopAwareDirection(snapshot,
                                    config,
                                    m_loopMemory,
                                    m_loopController,
                                    m_modePlanner,
                                    &m_lastDecisionSummary,
                                    true);
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
    m_modePlanner.clear();
    m_lastDecisionSummary.clear();
  }

private:
  mutable LoopMemory m_loopMemory;
  mutable LoopController m_loopController;
  mutable ModePlanner m_modePlanner;
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
