#include "core/session/core.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <deque>
#include <limits>
#include <tuple>
#include <utility>
#include <vector>

namespace nenoserpent::core {

namespace {
constexpr int PowerUpLifetimeTicks = 100;
constexpr int StallHashWindow = 128;
constexpr int StallNoScoreTicksThreshold = 120;
constexpr int StallRepeatThreshold = 6;
constexpr int SpawnMinHeadDistance = 2;
constexpr int SpawnMinObstacleDistance = 2;
constexpr int SpawnTopKMin = 3;
constexpr int SpawnTopKMax = 8;

enum class SpawnProfile {
  NoObstacle,
  CorridorObstacle,
  StaticObstacle,
  DynamicObstacle,
};

struct SpawnTuning {
  int minHeadDistance = SpawnMinHeadDistance;
  int minObstacleDistance = SpawnMinObstacleDistance;
  int topKMin = SpawnTopKMin;
  int topKMax = SpawnTopKMax;
  int freeNeighborWeight = 28;
  int reachableAreaWeight = 10;
  int tailReachableBonus = 120;
  int obstacleDistanceWeight = 8;
  int headDistanceWeight = 4;
  int centerBiasWeight = 0;
  int edgePenaltyWeight = 0;
  int dynamicRiskWeight = 0;
  int dynamicRiskHardLimit = 0;
  int dynamicRiskHorizon = 0;
  int recentSpawnPenaltyWeight = 0;
};

auto spawnTuningForProfile(SpawnProfile profile) -> SpawnTuning;
auto buildPredictedObstacleRisk(int boardWidth,
                                int boardHeight,
                                const QList<QPoint>& previousObstacles,
                                const QList<QPoint>& currentObstacles,
                                int horizonTicks) -> std::vector<int>;

auto boardIndex(const QPoint& p, const int boardWidth) -> int {
  return p.y() * boardWidth + p.x();
}

auto tryBoardIndex(const QPoint& p, const int boardWidth, const int boardHeight)
  -> std::optional<int> {
  if (p.x() < 0 || p.y() < 0 || p.x() >= boardWidth || p.y() >= boardHeight) {
    return std::nullopt;
  }
  return boardIndex(p, boardWidth);
}

auto toroidalDistance(const QPoint& a, const QPoint& b, const int boardWidth, const int boardHeight)
  -> int {
  const int dx = std::abs(a.x() - b.x());
  const int dy = std::abs(a.y() - b.y());
  return std::min(dx, boardWidth - dx) + std::min(dy, boardHeight - dy);
}

auto buildSpawnBlockedMap(const int boardWidth,
                          const int boardHeight,
                          const std::function<bool(const QPoint&)>& isBlocked)
  -> std::vector<bool> {
  std::vector<bool> blocked(static_cast<std::size_t>(boardWidth * boardHeight), false);
  for (int x = 0; x < boardWidth; ++x) {
    for (int y = 0; y < boardHeight; ++y) {
      const QPoint p{x, y};
      blocked[static_cast<std::size_t>(boardIndex(p, boardWidth))] = isBlocked(p);
    }
  }
  return blocked;
}

auto countFreeNeighbors(const QPoint& point,
                        const int boardWidth,
                        const int boardHeight,
                        const std::vector<bool>& blocked) -> int {
  constexpr std::array<QPoint, 4> kDirs = {
    QPoint{1, 0},
    QPoint{-1, 0},
    QPoint{0, 1},
    QPoint{0, -1},
  };
  int freeNeighbors = 0;
  for (const QPoint& d : kDirs) {
    const QPoint next = wrapPoint(point + d, boardWidth, boardHeight);
    const auto index = tryBoardIndex(next, boardWidth, boardHeight);
    if (!index.has_value()) {
      continue;
    }
    if (!blocked[static_cast<std::size_t>(*index)]) {
      ++freeNeighbors;
    }
  }
  return freeNeighbors;
}

auto bfsDistances(const QPoint& start,
                  const int boardWidth,
                  const int boardHeight,
                  const std::vector<bool>& blocked) -> std::vector<int> {
  std::vector<int> distances(blocked.size(), -1);
  const QPoint wrappedStart = wrapPoint(start, boardWidth, boardHeight);
  const auto startIndex = tryBoardIndex(wrappedStart, boardWidth, boardHeight);
  if (!startIndex.has_value()) {
    return distances;
  }
  std::deque<QPoint> queue;
  queue.push_back(wrappedStart);
  distances[static_cast<std::size_t>(*startIndex)] = 0;

  constexpr std::array<QPoint, 4> kDirs = {
    QPoint{1, 0},
    QPoint{-1, 0},
    QPoint{0, 1},
    QPoint{0, -1},
  };
  while (!queue.empty()) {
    const QPoint current = queue.front();
    queue.pop_front();
    const auto currentIndex = tryBoardIndex(current, boardWidth, boardHeight);
    if (!currentIndex.has_value()) {
      continue;
    }
    const int nextDistance = distances[static_cast<std::size_t>(*currentIndex)] + 1;
    for (const QPoint& d : kDirs) {
      const QPoint next = wrapPoint(current + d, boardWidth, boardHeight);
      const auto nextIndex = tryBoardIndex(next, boardWidth, boardHeight);
      if (!nextIndex.has_value()) {
        continue;
      }
      const auto idx = static_cast<std::size_t>(*nextIndex);
      if (blocked[idx] || distances[idx] >= 0) {
        continue;
      }
      distances[idx] = nextDistance;
      queue.push_back(next);
    }
  }
  return distances;
}

auto buildConnectedComponents(const int boardWidth,
                              const int boardHeight,
                              const std::vector<bool>& blocked) -> std::pair<std::vector<int>, std::vector<int>> {
  std::vector<int> componentOf(blocked.size(), -1);
  std::vector<int> componentSizes;
  int nextComponent = 0;
  constexpr std::array<QPoint, 4> kDirs = {
    QPoint{1, 0},
    QPoint{-1, 0},
    QPoint{0, 1},
    QPoint{0, -1},
  };

  for (int x = 0; x < boardWidth; ++x) {
    for (int y = 0; y < boardHeight; ++y) {
      const QPoint seed{x, y};
      const auto seedIndex = tryBoardIndex(seed, boardWidth, boardHeight);
      if (!seedIndex.has_value()) {
        continue;
      }
      const auto idx = static_cast<std::size_t>(*seedIndex);
      if (blocked[idx] || componentOf[idx] >= 0) {
        continue;
      }
      int size = 0;
      std::deque<QPoint> queue;
      queue.push_back(seed);
      componentOf[idx] = nextComponent;
      while (!queue.empty()) {
        const QPoint current = queue.front();
        queue.pop_front();
        ++size;
        for (const QPoint& d : kDirs) {
          const QPoint next = wrapPoint(current + d, boardWidth, boardHeight);
          const auto nextIndex = tryBoardIndex(next, boardWidth, boardHeight);
          if (!nextIndex.has_value()) {
            continue;
          }
          const auto nextIdx = static_cast<std::size_t>(*nextIndex);
          if (blocked[nextIdx] || componentOf[nextIdx] >= 0) {
            continue;
          }
          componentOf[nextIdx] = nextComponent;
          queue.push_back(next);
        }
      }
      componentSizes.push_back(size);
      ++nextComponent;
    }
  }
  return {componentOf, componentSizes};
}

struct SpawnCandidate {
  QPoint point{0, 0};
  int score = std::numeric_limits<int>::min();
};

auto pickSpawnPointWithSafety(const int boardWidth,
                              const int boardHeight,
                              const QPoint& head,
                              const std::optional<QPoint>& tail,
                              const QList<QPoint>& obstacles,
                              const QList<QPoint>& previousObstacles,
                              const std::deque<QPoint>& recentSpawnPoints,
                              const SpawnProfile profile,
                              const std::function<bool(const QPoint&)>& isBlocked,
                              const std::function<int(int)>& randomBounded,
                              QPoint& pickedPoint) -> bool {
  const SpawnTuning tuning = spawnTuningForProfile(profile);
  const auto predictedRisk = buildPredictedObstacleRisk(boardWidth,
                                                        boardHeight,
                                                        previousObstacles,
                                                        obstacles,
                                                        tuning.dynamicRiskHorizon);
  const QList<QPoint> freeSpots = collectFreeSpots(boardWidth, boardHeight, isBlocked);
  if (freeSpots.isEmpty()) {
    return false;
  }

  auto blocked = buildSpawnBlockedMap(boardWidth, boardHeight, isBlocked);
  if (const auto headIndex = tryBoardIndex(wrapPoint(head, boardWidth, boardHeight),
                                           boardWidth,
                                           boardHeight);
      headIndex.has_value()) {
    blocked[static_cast<std::size_t>(*headIndex)] = false;
  }

  const auto distanceFromHead = bfsDistances(head, boardWidth, boardHeight, blocked);
  const auto [componentOf, componentSizes] =
    buildConnectedComponents(boardWidth, boardHeight, blocked);
  int tailComponent = -1;
  if (tail.has_value()) {
    if (const auto tailIndex = tryBoardIndex(wrapPoint(*tail, boardWidth, boardHeight),
                                             boardWidth,
                                             boardHeight);
        tailIndex.has_value()) {
      tailComponent = componentOf[static_cast<std::size_t>(*tailIndex)];
    }
  }

  auto minObstacleDistance = [&](const QPoint& point) -> int {
    if (obstacles.isEmpty()) {
      return std::numeric_limits<int>::max();
    }
    int minDistance = std::numeric_limits<int>::max();
    for (const QPoint& obstacle : obstacles) {
      minDistance =
        std::min(minDistance, toroidalDistance(point, obstacle, boardWidth, boardHeight));
    }
    return minDistance;
  };

  auto gatherCandidates = [&](const bool requireDistances,
                              const bool requirePocketFilter,
                              const bool requireTailReachable) {
    std::vector<SpawnCandidate> candidates;
    candidates.reserve(static_cast<std::size_t>(freeSpots.size()));
    for (const QPoint& point : freeSpots) {
      const auto pointIndex = tryBoardIndex(point, boardWidth, boardHeight);
      if (!pointIndex.has_value()) {
        continue;
      }
      const auto idx = static_cast<std::size_t>(*pointIndex);
      const int reachableDistance = distanceFromHead[idx];
      if (reachableDistance < 0) {
        continue;
      }
      const int dynamicRisk = predictedRisk[idx];
      if (requireDistances && tuning.dynamicRiskHardLimit > 0 &&
          dynamicRisk >= tuning.dynamicRiskHardLimit) {
        continue;
      }
      const int freeNeighbors = countFreeNeighbors(point, boardWidth, boardHeight, blocked);
      if (requirePocketFilter && freeNeighbors < 2) {
        continue;
      }
      const int obstacleDistance = minObstacleDistance(point);
      const int headDistance = toroidalDistance(point, head, boardWidth, boardHeight);
      if (requireDistances && headDistance < tuning.minHeadDistance) {
        continue;
      }
      if (requireDistances && obstacleDistance != std::numeric_limits<int>::max() &&
          obstacleDistance < tuning.minObstacleDistance) {
        continue;
      }
      const int component = componentOf[idx];
      if (component < 0) {
        continue;
      }
      const bool tailReachable = tailComponent >= 0 && component == tailComponent;
      if (requireTailReachable && tailComponent >= 0 && !tailReachable) {
        continue;
      }

      const int reachableArea = componentSizes[static_cast<std::size_t>(component)];
      int score =
        (reachableArea * tuning.reachableAreaWeight) + (freeNeighbors * tuning.freeNeighborWeight);
      if (tailReachable) {
        score += tuning.tailReachableBonus;
      }
      if (obstacleDistance != std::numeric_limits<int>::max()) {
        score += std::min(obstacleDistance, 6) * tuning.obstacleDistanceWeight;
      }
      score += std::min(headDistance, 6) * tuning.headDistanceWeight;

      const int centerX2 = boardWidth - 1;
      const int centerY2 = boardHeight - 1;
      const int dx2 = std::abs((point.x() * 2) - centerX2);
      const int dy2 = std::abs((point.y() * 2) - centerY2);
      const int centerDistance2 = dx2 + dy2;
      const int maxCenterDistance2 = centerX2 + centerY2;
      score += (maxCenterDistance2 - centerDistance2) * tuning.centerBiasWeight;

      const bool onEdge = point.x() == 0 || point.y() == 0 || point.x() == boardWidth - 1 ||
                          point.y() == boardHeight - 1;
      if (onEdge) {
        score -= tuning.edgePenaltyWeight * 10;
      }
      score -= dynamicRisk * tuning.dynamicRiskWeight;
      int recentPenalty = 0;
      for (const QPoint& recent : recentSpawnPoints) {
        const int d = toroidalDistance(point, recent, boardWidth, boardHeight);
        if (d == 0) {
          recentPenalty += 16;
        } else if (d <= 1) {
          recentPenalty += 10;
        } else if (d <= 2) {
          recentPenalty += 4;
        }
      }
      score -= recentPenalty * tuning.recentSpawnPenaltyWeight;
      candidates.push_back({.point = point, .score = score});
    }
    std::sort(candidates.begin(), candidates.end(), [](const SpawnCandidate& a,
                                                       const SpawnCandidate& b) {
      if (a.score != b.score) {
        return a.score > b.score;
      }
      if (a.point.x() != b.point.x()) {
        return a.point.x() < b.point.x();
      }
      return a.point.y() < b.point.y();
    });
    return candidates;
  };

  const std::array<std::tuple<bool, bool, bool>, 4> passes = {
    std::tuple{true, true, true},
    std::tuple{true, true, false},
    std::tuple{false, true, false},
    std::tuple{false, false, false},
  };
  for (const auto& [requireDistances, requirePocketFilter, requireTailReachable] : passes) {
    auto candidates =
      gatherCandidates(requireDistances, requirePocketFilter, requireTailReachable);
    if (candidates.empty()) {
      continue;
    }
    const int topK =
      std::clamp(static_cast<int>(candidates.size()) / 3, tuning.topKMin, tuning.topKMax);
    const int selected = randomBounded(topK);
    if (selected < 0 || selected >= topK) {
      return false;
    }
    pickedPoint = candidates[static_cast<std::size_t>(selected)].point;
    return true;
  }

  const int selected = randomBounded(freeSpots.size());
  if (selected < 0 || selected >= freeSpots.size()) {
    return false;
  }
  pickedPoint = freeSpots[selected];
  return true;
}

void rememberRecentSpawnPoint(std::deque<QPoint>& recentSpawnPoints, const QPoint point) {
  constexpr std::size_t kRecentSpawnWindow = 18;
  recentSpawnPoints.push_back(point);
  while (recentSpawnPoints.size() > kRecentSpawnWindow) {
    recentSpawnPoints.pop_front();
  }
}

auto mixHash(std::uint64_t seed, const std::uint64_t value) -> std::uint64_t {
  constexpr std::uint64_t kPrime = 1099511628211ULL;
  seed ^= value + 0x9e3779b97f4a7c15ULL + (seed << 6U) + (seed >> 2U);
  seed *= kPrime;
  return seed;
}

auto spawnTuningForProfile(const SpawnProfile profile) -> SpawnTuning {
  switch (profile) {
  case SpawnProfile::NoObstacle:
    return SpawnTuning{
      .minHeadDistance = 1,
      .minObstacleDistance = 0,
      .topKMin = 3,
      .topKMax = 8,
      .freeNeighborWeight = 24,
      .reachableAreaWeight = 8,
      .tailReachableBonus = 90,
      .obstacleDistanceWeight = 0,
      .headDistanceWeight = 3,
      .centerBiasWeight = 4,
      .edgePenaltyWeight = 2,
      .dynamicRiskWeight = 0,
      .dynamicRiskHardLimit = 0,
      .dynamicRiskHorizon = 0,
      .recentSpawnPenaltyWeight = 10,
    };
  case SpawnProfile::CorridorObstacle:
    return SpawnTuning{
      .minHeadDistance = 2,
      .minObstacleDistance = 1,
      .topKMin = 5,
      .topKMax = 10,
      .freeNeighborWeight = 33,
      .reachableAreaWeight = 13,
      .tailReachableBonus = 180,
      .obstacleDistanceWeight = 8,
      .headDistanceWeight = 4,
      .centerBiasWeight = 9,
      .edgePenaltyWeight = 10,
      .dynamicRiskWeight = 1,
      .dynamicRiskHardLimit = 60,
      .dynamicRiskHorizon = 2,
      .recentSpawnPenaltyWeight = 20,
    };
  case SpawnProfile::StaticObstacle:
    return SpawnTuning{
      .minHeadDistance = 2,
      .minObstacleDistance = 2,
      .topKMin = 4,
      .topKMax = 8,
      .freeNeighborWeight = 30,
      .reachableAreaWeight = 11,
      .tailReachableBonus = 140,
      .obstacleDistanceWeight = 9,
      .headDistanceWeight = 4,
      .centerBiasWeight = 6,
      .edgePenaltyWeight = 4,
      .dynamicRiskWeight = 1,
      .dynamicRiskHardLimit = 80,
      .dynamicRiskHorizon = 3,
      .recentSpawnPenaltyWeight = 14,
    };
  case SpawnProfile::DynamicObstacle:
    return SpawnTuning{
      .minHeadDistance = 2,
      .minObstacleDistance = 1,
      .topKMin = 5,
      .topKMax = 10,
      .freeNeighborWeight = 34,
      .reachableAreaWeight = 13,
      .tailReachableBonus = 170,
      .obstacleDistanceWeight = 10,
      .headDistanceWeight = 4,
      .centerBiasWeight = 12,
      .edgePenaltyWeight = 8,
      .dynamicRiskWeight = 6,
      .dynamicRiskHardLimit = 42,
      .dynamicRiskHorizon = 8,
      .recentSpawnPenaltyWeight = 12,
    };
  }
  return SpawnTuning{};
}

auto obstacleSignature(const QList<QPoint>& obstacles, const int boardWidth, const int boardHeight)
  -> std::uint64_t {
  std::vector<QPoint> points;
  points.reserve(static_cast<std::size_t>(obstacles.size()));
  for (const QPoint& obstacle : obstacles) {
    points.push_back(wrapPoint(obstacle, boardWidth, boardHeight));
  }
  std::sort(points.begin(), points.end(), [](const QPoint& a, const QPoint& b) {
    if (a.x() != b.x()) {
      return a.x() < b.x();
    }
    return a.y() < b.y();
  });

  std::uint64_t hash = 1469598103934665603ULL;
  hash = mixHash(hash, static_cast<std::uint64_t>(points.size()));
  for (const QPoint& point : points) {
    hash = mixHash(hash, static_cast<std::uint64_t>(point.x() + 1024));
    hash = mixHash(hash, static_cast<std::uint64_t>(point.y() + 1024));
  }
  return hash;
}

auto classifySpawnProfile(const QList<QPoint>& obstacles,
                          const int boardWidth,
                          const int boardHeight,
                          std::uint64_t& lastSignature,
                          bool& hasLastSignature,
                          int& dynamicConfidenceTicks) -> SpawnProfile {
  if (obstacles.isEmpty()) {
    hasLastSignature = false;
    lastSignature = 0;
    dynamicConfidenceTicks = 0;
    return SpawnProfile::NoObstacle;
  }

  const auto signature = obstacleSignature(obstacles, boardWidth, boardHeight);
  if (hasLastSignature && signature != lastSignature) {
    dynamicConfidenceTicks = std::min(dynamicConfidenceTicks + 8, 64);
  } else {
    dynamicConfidenceTicks = std::max(dynamicConfidenceTicks - 1, 0);
  }
  hasLastSignature = true;
  lastSignature = signature;

  if (dynamicConfidenceTicks >= 4) {
    return SpawnProfile::DynamicObstacle;
  }
  if (obstacles.size() >= 12) {
    std::vector<bool> usedX(static_cast<std::size_t>(boardWidth), false);
    std::vector<bool> usedY(static_cast<std::size_t>(boardHeight), false);
    int uniqueX = 0;
    int uniqueY = 0;
    for (const QPoint& obstacle : obstacles) {
      const QPoint wrapped = wrapPoint(obstacle, boardWidth, boardHeight);
      if (!usedX[static_cast<std::size_t>(wrapped.x())]) {
        usedX[static_cast<std::size_t>(wrapped.x())] = true;
        ++uniqueX;
      }
      if (!usedY[static_cast<std::size_t>(wrapped.y())]) {
        usedY[static_cast<std::size_t>(wrapped.y())] = true;
        ++uniqueY;
      }
    }
    if (uniqueX <= 3 || uniqueY <= 3) {
      return SpawnProfile::CorridorObstacle;
    }
  }
  return SpawnProfile::StaticObstacle;
}

auto signedToroidalDelta(const int from, const int to, const int size) -> int {
  int delta = (to - from) % size;
  if (delta < -size / 2) {
    delta += size;
  } else if (delta > size / 2) {
    delta -= size;
  }
  return delta;
}

auto buildPredictedObstacleRisk(const int boardWidth,
                                const int boardHeight,
                                const QList<QPoint>& previousObstacles,
                                const QList<QPoint>& currentObstacles,
                                const int horizonTicks) -> std::vector<int> {
  std::vector<int> risk(static_cast<std::size_t>(boardWidth * boardHeight), 0);
  if (horizonTicks <= 0 || previousObstacles.isEmpty() || currentObstacles.isEmpty()) {
    return risk;
  }

  std::vector<bool> prevUsed(static_cast<std::size_t>(previousObstacles.size()), false);
  for (const QPoint& current : currentObstacles) {
    int bestPrevIndex = -1;
    int bestDistance = std::numeric_limits<int>::max();
    for (int i = 0; i < previousObstacles.size(); ++i) {
      if (prevUsed[static_cast<std::size_t>(i)]) {
        continue;
      }
      const int distance =
        toroidalDistance(current, previousObstacles[i], boardWidth, boardHeight);
      if (distance < bestDistance) {
        bestDistance = distance;
        bestPrevIndex = i;
      }
    }
    if (bestPrevIndex < 0 || bestDistance > 3) {
      continue;
    }
    prevUsed[static_cast<std::size_t>(bestPrevIndex)] = true;
    const QPoint previous = previousObstacles[bestPrevIndex];
    const int dx = signedToroidalDelta(previous.x(), current.x(), boardWidth);
    const int dy = signedToroidalDelta(previous.y(), current.y(), boardHeight);
    if (dx == 0 && dy == 0) {
      continue;
    }

    for (int t = 1; t <= horizonTicks; ++t) {
      const QPoint projected =
        wrapPoint(QPoint(current.x() + (dx * t), current.y() + (dy * t)), boardWidth, boardHeight);
      const auto index = tryBoardIndex(projected, boardWidth, boardHeight);
      if (!index.has_value()) {
        continue;
      }
      risk[static_cast<std::size_t>(*index)] += (horizonTicks - t + 1) * 8;
    }
  }
  return risk;
}
} // namespace

auto MetaAction::resetTransientRuntime() -> MetaAction {
  return {
    .kind = Kind::ResetTransientRuntime,
    .obstacles = {},
    .snapshot = {},
    .previewSeed = {},
    .boardWidth = 0,
    .boardHeight = 0,
  };
}

auto MetaAction::resetReplayRuntime() -> MetaAction {
  return {
    .kind = Kind::ResetReplayRuntime,
    .obstacles = {},
    .snapshot = {},
    .previewSeed = {},
    .boardWidth = 0,
    .boardHeight = 0,
  };
}

auto MetaAction::bootstrapForLevel(QList<QPoint> obstacles,
                                   const int boardWidth,
                                   const int boardHeight) -> MetaAction {
  return {
    .kind = Kind::BootstrapForLevel,
    .obstacles = std::move(obstacles),
    .snapshot = {},
    .previewSeed = {},
    .boardWidth = boardWidth,
    .boardHeight = boardHeight,
  };
}

auto MetaAction::restorePersistedSession(const StateSnapshot& snapshot) -> MetaAction {
  return {
    .kind = Kind::RestorePersistedSession,
    .obstacles = {},
    .snapshot = snapshot,
    .previewSeed = {},
    .boardWidth = 0,
    .boardHeight = 0,
  };
}

auto MetaAction::seedPreviewState(const PreviewSeed& seed) -> MetaAction {
  return {
    .kind = Kind::SeedPreviewState,
    .obstacles = {},
    .snapshot = {},
    .previewSeed = seed,
    .boardWidth = 0,
    .boardHeight = 0,
  };
}

auto MetaAction::restoreSnapshot(const StateSnapshot& snapshot) -> MetaAction {
  return {
    .kind = Kind::RestoreSnapshot,
    .obstacles = {},
    .snapshot = snapshot,
    .previewSeed = {},
    .boardWidth = 0,
    .boardHeight = 0,
  };
}

void SessionCore::setDirection(const QPoint& direction) {
  m_state.direction = direction;
}

auto SessionCore::direction() const -> QPoint {
  return m_state.direction;
}

auto SessionCore::tickCounter() const -> int {
  return m_state.tickCounter;
}

auto SessionCore::currentTickIntervalMs() const -> int {
  if (m_state.activeBuff == static_cast<int>(BuffId::Slow)) {
    return 250;
  }
  return tickIntervalForScore(m_state.score);
}

auto SessionCore::headPosition() const -> QPoint {
  return m_body.empty() ? QPoint() : m_body.front();
}

void SessionCore::incrementTick() {
  m_state.tickCounter++;
}

void SessionCore::resetStallGuard() {
  m_stallNoScoreTicks = 0;
  m_stallLastScore = m_state.score;
  m_stallRecentHashes.clear();
  m_stallHashCounts.clear();
}

auto SessionCore::stallStateHash() const -> std::uint64_t {
  std::uint64_t hash = 1469598103934665603ULL;
  hash = mixHash(hash, static_cast<std::uint64_t>(headPosition().x() + 1024));
  hash = mixHash(hash, static_cast<std::uint64_t>(headPosition().y() + 1024));
  hash = mixHash(hash, static_cast<std::uint64_t>(m_state.direction.x() + 16));
  hash = mixHash(hash, static_cast<std::uint64_t>(m_state.direction.y() + 16));
  hash = mixHash(hash, static_cast<std::uint64_t>(m_state.food.x() + 1024));
  hash = mixHash(hash, static_cast<std::uint64_t>(m_state.food.y() + 1024));
  hash = mixHash(hash, static_cast<std::uint64_t>(m_state.powerUpPos.x() + 1024));
  hash = mixHash(hash, static_cast<std::uint64_t>(m_state.powerUpPos.y() + 1024));
  hash = mixHash(hash, static_cast<std::uint64_t>(m_state.powerUpType + 32));
  hash = mixHash(hash, static_cast<std::uint64_t>(m_body.size()));
  for (const QPoint& segment : m_body) {
    hash = mixHash(hash, static_cast<std::uint64_t>(segment.x() + 1024));
    hash = mixHash(hash, static_cast<std::uint64_t>(segment.y() + 1024));
  }
  return hash;
}

auto SessionCore::observeStallStateAndMaybeResetTarget(const SessionAdvanceConfig& config,
                                                       const std::function<int(int)>& randomBounded,
                                                       SessionAdvanceResult& result) -> bool {
  if (!config.consumeInputQueue || m_body.empty() || m_state.score != m_stallLastScore) {
    resetStallGuard();
    return false;
  }

  ++m_stallNoScoreTicks;
  const std::uint64_t hash = stallStateHash();
  const int repeats = ++m_stallHashCounts[hash];
  m_stallRecentHashes.push_back(hash);
  while (static_cast<int>(m_stallRecentHashes.size()) > StallHashWindow) {
    const std::uint64_t dropped = m_stallRecentHashes.front();
    m_stallRecentHashes.pop_front();
    auto it = m_stallHashCounts.find(dropped);
    if (it == m_stallHashCounts.end()) {
      continue;
    }
    --it->second;
    if (it->second <= 0) {
      m_stallHashCounts.erase(it);
    }
  }

  if (m_stallNoScoreTicks < StallNoScoreTicksThreshold || repeats < StallRepeatThreshold) {
    return false;
  }

  const QPoint oldFood = m_state.food;
  bool changed = false;
  for (int attempt = 0; attempt < 3; ++attempt) {
    if (!spawnFood(config.boardWidth, config.boardHeight, randomBounded)) {
      break;
    }
    if (m_state.food != oldFood) {
      changed = true;
      break;
    }
  }
  if (!changed) {
    return false;
  }

  result.movedFood = true;
  result.targetReset = true;
  resetStallGuard();
  return true;
}

auto SessionCore::enqueueDirection(const QPoint& direction, const std::size_t maxQueueSize)
  -> bool {
  if (m_inputQueue.size() >= maxQueueSize) {
    return false;
  }

  const QPoint lastDirection = m_inputQueue.empty() ? m_state.direction : m_inputQueue.back();
  if (((direction.x() != 0) && lastDirection.x() == -direction.x()) ||
      ((direction.y() != 0) && lastDirection.y() == -direction.y())) {
    return false;
  }

  m_inputQueue.push_back(direction);
  return true;
}

auto SessionCore::consumeQueuedInput(QPoint& nextInput) -> bool {
  if (m_inputQueue.empty()) {
    return false;
  }

  nextInput = m_inputQueue.front();
  m_inputQueue.pop_front();
  return true;
}

void SessionCore::clearQueuedInput() {
  m_inputQueue.clear();
}

void SessionCore::setBody(const std::deque<QPoint>& body) {
  m_body = body;
}

void SessionCore::applyMovement(const QPoint& newHead, const bool grew) {
  m_body.push_front(newHead);
  if (!grew && !m_body.empty()) {
    m_body.pop_back();
  }
}

auto SessionCore::checkCollision(const QPoint& head, const int boardWidth, const int boardHeight)
  -> CollisionOutcome {
  const auto outcome =
    collisionOutcomeForHead(head,
                            boardWidth,
                            boardHeight,
                            m_state.obstacles,
                            m_body,
                            m_state.activeBuff == static_cast<int>(BuffId::Ghost),
                            m_state.activeBuff == static_cast<int>(BuffId::Portal),
                            m_state.activeBuff == static_cast<int>(BuffId::Laser),
                            m_state.shieldActive);

  if (outcome.consumeLaser && outcome.obstacleIndex >= 0 &&
      outcome.obstacleIndex < m_state.obstacles.size()) {
    m_state.obstacles.removeAt(outcome.obstacleIndex);
    m_state.activeBuff = static_cast<int>(BuffId::None);
  }

  if (outcome.consumeShield) {
    m_state.shieldActive = false;
  }

  return outcome;
}

auto SessionCore::consumeFood(const QPoint& head,
                              const int boardWidth,
                              const int boardHeight,
                              const std::function<int(int)>& randomBounded)
  -> FoodConsumptionResult {
  const auto result = planFoodConsumption(head, m_state, boardWidth, boardHeight, randomBounded);
  if (!result.ate) {
    return result;
  }

  m_state.score = result.newScore;
  if (result.triggerChoice) {
    m_state.lastRoguelikeChoiceScore = m_state.score;
  }
  return result;
}

auto SessionCore::consumePowerUp(const QPoint& head,
                                 const int baseDurationTicks,
                                 const bool halfDurationForRich) -> PowerUpConsumptionResult {
  const auto result = planPowerUpConsumption(head, m_state, baseDurationTicks, halfDurationForRich);
  if (!result.ate) {
    return result;
  }

  applyPowerUpResult(result);
  m_state.powerUpPos = QPoint(-1, -1);
  m_state.powerUpTicksRemaining = 0;
  return result;
}

auto SessionCore::applyChoiceSelection(const int powerUpType,
                                       const int baseDurationTicks,
                                       const bool halfDurationForRich) -> PowerUpConsumptionResult {
  m_state.lastRoguelikeChoiceScore = m_state.score;
  const auto result = planPowerUpAcquisition(powerUpType, baseDurationTicks, halfDurationForRich);
  applyPowerUpResult(result);
  return result;
}

auto SessionCore::selectChoice(const int powerUpType,
                               const int baseDurationTicks,
                               const bool halfDurationForRich) -> PowerUpConsumptionResult {
  return applyChoiceSelection(powerUpType, baseDurationTicks, halfDurationForRich);
}

auto SessionCore::tickBuffCountdown() -> bool {
  if (m_state.activeBuff == static_cast<int>(BuffId::None) ||
      !nenoserpent::core::tickBuffCountdown(m_state.buffTicksRemaining)) {
    return false;
  }

  m_state.activeBuff = static_cast<int>(BuffId::None);
  m_state.buffTicksRemaining = 0;
  m_state.buffTicksTotal = 0;
  m_state.shieldActive = false;
  return true;
}

auto SessionCore::spawnFood(const int boardWidth,
                            const int boardHeight,
                            const std::function<int(int)>& randomBounded) -> bool {
  QPoint pickedPoint;
  const std::optional<QPoint> tail = m_body.empty() ? std::nullopt : std::optional{m_body.back()};
  const SpawnProfile profile = classifySpawnProfile(m_state.obstacles,
                                                    boardWidth,
                                                    boardHeight,
                                                    m_lastObstacleSignature,
                                                    m_hasLastObstacleSignature,
                                                    m_dynamicObstacleConfidenceTicks);
  const bool found = pickSpawnPointWithSafety(boardWidth,
                                              boardHeight,
                                              headPosition(),
                                              tail,
                                              m_state.obstacles,
                                              m_prevObstacleSnapshot,
                                              m_recentSpawnPoints,
                                              profile,
                                              [this](const QPoint& point) -> bool {
                                                return isOccupied(point) || point == m_state.powerUpPos;
                                              },
                                              randomBounded,
                                              pickedPoint);
  if (found) {
    m_state.food = pickedPoint;
    rememberRecentSpawnPoint(m_recentSpawnPoints, pickedPoint);
  }
  return found;
}

auto SessionCore::spawnPowerUp(const int boardWidth,
                               const int boardHeight,
                               const std::function<int(int)>& randomBounded) -> bool {
  QPoint pickedPoint;
  const std::optional<QPoint> tail = m_body.empty() ? std::nullopt : std::optional{m_body.back()};
  const SpawnProfile profile = classifySpawnProfile(m_state.obstacles,
                                                    boardWidth,
                                                    boardHeight,
                                                    m_lastObstacleSignature,
                                                    m_hasLastObstacleSignature,
                                                    m_dynamicObstacleConfidenceTicks);
  const bool found = pickSpawnPointWithSafety(boardWidth,
                                              boardHeight,
                                              headPosition(),
                                              tail,
                                              m_state.obstacles,
                                              m_prevObstacleSnapshot,
                                              m_recentSpawnPoints,
                                              profile,
                                              [this](const QPoint& point) -> bool {
                                                return isOccupied(point) || point == m_state.food;
                                              },
                                              randomBounded,
                                              pickedPoint);
  if (found) {
    m_state.powerUpPos = pickedPoint;
    m_state.powerUpType = static_cast<int>(weightedRandomBuffId(randomBounded));
    m_state.powerUpTicksRemaining = PowerUpLifetimeTicks;
    rememberRecentSpawnPoint(m_recentSpawnPoints, pickedPoint);
  }
  return found;
}

auto SessionCore::applyMagnetAttraction(const int boardWidth, const int boardHeight)
  -> MagnetAttractionResult {
  if (m_state.activeBuff != static_cast<int>(BuffId::Magnet) || m_state.food == QPoint(-1, -1) ||
      m_body.empty()) {
    return {};
  }

  auto result = nenoserpent::core::applyMagnetAttraction(
    headPosition(), boardWidth, boardHeight, m_state, [this](const QPoint& pos) {
      return isOccupied(pos);
    });
  if (result.moved) {
    m_state.food = result.newFood;
  }
  return result;
}

auto SessionCore::applyReplayTimeline(const QList<ReplayFrame>& inputFrames,
                                      int& inputHistoryIndex,
                                      const QList<ChoiceRecord>& choiceFrames,
                                      int& choiceHistoryIndex) -> ReplayTimelineApplication {
  ReplayTimelineApplication result;

  while (inputHistoryIndex < inputFrames.size()) {
    const auto& frame = inputFrames[inputHistoryIndex];
    if (frame.frame == m_state.tickCounter) {
      result.appliedInput = true;
      result.appliedDirection = QPoint(frame.dx, frame.dy);
      setDirection(result.appliedDirection);
      inputHistoryIndex++;
    } else if (frame.frame > m_state.tickCounter) {
      break;
    } else {
      inputHistoryIndex++;
    }
  }

  while (choiceHistoryIndex < choiceFrames.size()) {
    const auto& frame = choiceFrames[choiceHistoryIndex];
    if (frame.frame == m_state.tickCounter) {
      result.choiceIndex = frame.index;
      choiceHistoryIndex++;
      break;
    }
    if (frame.frame > m_state.tickCounter) {
      break;
    }
    choiceHistoryIndex++;
  }

  return result;
}

auto SessionCore::beginRuntimeUpdate() -> RuntimeUpdateResult {
  return {
    .buffExpired = tickBuffCountdown(),
    .powerUpExpired = tickPowerUpCountdown(),
  };
}

void SessionCore::finishRuntimeUpdate() {
  incrementTick();
}

auto SessionCore::tick(const TickCommand& command, const std::function<int(int)>& randomBounded)
  -> TickResult {
  TickResult result;

  if (command.applyRuntimeHooks) {
    result.runtimeUpdate = beginRuntimeUpdate();
  }

  if (command.replayInputFrames != nullptr && command.replayInputHistoryIndex != nullptr &&
      command.replayChoiceFrames != nullptr && command.replayChoiceHistoryIndex != nullptr) {
    result.replayTimeline = applyReplayTimeline(*command.replayInputFrames,
                                                *command.replayInputHistoryIndex,
                                                *command.replayChoiceFrames,
                                                *command.replayChoiceHistoryIndex);
  }

  result.step = advanceSessionStep(command.advanceConfig, randomBounded);

  if (command.applyRuntimeHooks) {
    finishRuntimeUpdate();
  }

  return result;
}

void SessionCore::applyMetaAction(const MetaAction& action) {
  switch (action.kind) {
  case MetaAction::Kind::ResetTransientRuntime:
    resetTransientRuntimeState();
    break;
  case MetaAction::Kind::ResetReplayRuntime:
    resetReplayRuntimeState();
    break;
  case MetaAction::Kind::BootstrapForLevel:
    bootstrapForLevel(action.obstacles, action.boardWidth, action.boardHeight);
    break;
  case MetaAction::Kind::RestorePersistedSession:
    restorePersistedSession(action.snapshot);
    break;
  case MetaAction::Kind::SeedPreviewState:
    seedPreviewState(action.previewSeed);
    break;
  case MetaAction::Kind::RestoreSnapshot:
    restoreSnapshot(action.snapshot);
    break;
  }
}

auto SessionCore::advanceSessionStep(const SessionAdvanceConfig& config,
                                     const std::function<int(int)>& randomBounded)
  -> SessionAdvanceResult {
  SessionAdvanceResult result;

  if (!m_hasObstacleSnapshots) {
    m_prevObstacleSnapshot = m_state.obstacles;
    m_currObstacleSnapshot = m_state.obstacles;
    m_hasObstacleSnapshots = true;
  } else if (m_currObstacleSnapshot != m_state.obstacles) {
    m_prevObstacleSnapshot = m_currObstacleSnapshot;
    m_currObstacleSnapshot = m_state.obstacles;
  }

  if (config.consumeInputQueue) {
    QPoint nextInput;
    if (consumeQueuedInput(nextInput)) {
      setDirection(nextInput);
      result.consumedInput = true;
      result.consumedDirection = nextInput;
    }
  }

  result.nextHead = headPosition() + direction();
  const QPoint wrappedHead = wrapPoint(result.nextHead, config.boardWidth, config.boardHeight);
  const auto collisionOutcome =
    checkCollision(result.nextHead, config.boardWidth, config.boardHeight);
  result.consumeShield = collisionOutcome.consumeShield;
  result.consumeLaser = collisionOutcome.consumeLaser;
  result.obstacleIndex = collisionOutcome.obstacleIndex;
  if (collisionOutcome.collision) {
    result.collision = true;
    return result;
  }

  result.grew = (wrappedHead == m_state.food);

  const auto foodResult =
    consumeFood(result.nextHead, config.boardWidth, config.boardHeight, randomBounded);
  result.ateFood = foodResult.ate;
  result.triggerChoice = foodResult.triggerChoice;
  result.spawnPowerUp = foodResult.spawnPowerUp;
  result.foodPan = foodResult.pan;
  if (result.triggerChoice && config.pauseOnChoiceTrigger) {
    return result;
  }

  const auto powerResult = consumePowerUp(wrappedHead, 40, true);
  result.atePowerUp = powerResult.ate;
  result.miniApplied = powerResult.miniApplied;
  result.slowMode = powerResult.slowMode;

  applyMovement(wrappedHead, result.grew);
  result.appliedMovement = true;

  const auto magnetResult = applyMagnetAttraction(config.boardWidth, config.boardHeight);
  result.movedFood = magnetResult.moved;
  result.magnetAteFood = magnetResult.ate;
  if (magnetResult.ate) {
    const auto magnetFoodResult =
      consumeFood(headPosition(), config.boardWidth, config.boardHeight, randomBounded);
    result.triggerChoiceAfterMagnet = magnetFoodResult.triggerChoice;
    result.spawnPowerUpAfterMagnet = magnetFoodResult.spawnPowerUp;
    result.magnetFoodPan = magnetFoodResult.pan;
  }

  observeStallStateAndMaybeResetTarget(config, randomBounded, result);

  return result;
}

void SessionCore::bootstrapForLevel(QList<QPoint> obstacles,
                                    const int boardWidth,
                                    const int boardHeight) {
  m_state = {};
  m_state.direction = {0, -1};
  m_state.powerUpPos = QPoint(-1, -1);
  m_state.lastRoguelikeChoiceScore = -1000;
  m_state.obstacles = std::move(obstacles);
  m_body = buildSafeInitialSnakeBody(m_state.obstacles, boardWidth, boardHeight);
  m_inputQueue.clear();
  m_hasLastObstacleSignature = false;
  m_lastObstacleSignature = 0;
  m_dynamicObstacleConfidenceTicks = 0;
  m_prevObstacleSnapshot.clear();
  m_currObstacleSnapshot.clear();
  m_hasObstacleSnapshots = false;
  m_recentSpawnPoints.clear();
  resetStallGuard();
}

void SessionCore::restorePersistedSession(const StateSnapshot& snapshot) {
  m_state = snapshot.state;
  m_body = snapshot.body;
  m_inputQueue.clear();

  const QPoint persistedDirection = m_state.direction;
  const QPoint persistedFood = m_state.food;
  const int persistedScore = m_state.score;
  const QList<QPoint> persistedObstacles = m_state.obstacles;

  resetTransientRuntimeState();
  resetReplayRuntimeState();

  m_state.direction = persistedDirection;
  m_state.food = persistedFood;
  m_state.score = persistedScore;
  m_state.obstacles = persistedObstacles;
  resetStallGuard();
}

void SessionCore::seedPreviewState(const PreviewSeed& seed) {
  m_state = {};
  m_state.food = seed.food;
  m_state.direction = seed.direction;
  m_state.powerUpPos = seed.powerUpPos;
  m_state.powerUpType = seed.powerUpType;
  m_state.powerUpTicksRemaining = seed.powerUpTicksRemaining;
  m_state.score = seed.score;
  m_state.obstacles = seed.obstacles;
  m_state.tickCounter = seed.tickCounter;
  m_state.activeBuff = seed.activeBuff;
  m_state.buffTicksRemaining = seed.buffTicksRemaining;
  m_state.buffTicksTotal = seed.buffTicksTotal;
  m_state.shieldActive = seed.shieldActive;
  m_state.lastRoguelikeChoiceScore = -1000;
  m_body = seed.body;
  m_inputQueue.clear();
  m_hasLastObstacleSignature = false;
  m_lastObstacleSignature = 0;
  m_dynamicObstacleConfidenceTicks = 0;
  m_prevObstacleSnapshot.clear();
  m_currObstacleSnapshot.clear();
  m_hasObstacleSnapshots = false;
  m_recentSpawnPoints.clear();
  resetStallGuard();
}

void SessionCore::resetTransientRuntimeState() {
  m_state.direction = {0, -1};
  m_inputQueue.clear();
  m_state.activeBuff = 0;
  m_state.buffTicksRemaining = 0;
  m_state.buffTicksTotal = 0;
  m_state.shieldActive = false;
  m_state.powerUpPos = QPoint(-1, -1);
  m_state.powerUpType = 0;
  m_state.powerUpTicksRemaining = 0;
  m_hasLastObstacleSignature = false;
  m_lastObstacleSignature = 0;
  m_dynamicObstacleConfidenceTicks = 0;
  m_prevObstacleSnapshot.clear();
  m_currObstacleSnapshot.clear();
  m_hasObstacleSnapshots = false;
  m_recentSpawnPoints.clear();
  resetStallGuard();
}

void SessionCore::resetReplayRuntimeState() {
  m_state.tickCounter = 0;
  m_state.lastRoguelikeChoiceScore = -1000;
}

auto SessionCore::snapshot(const std::deque<QPoint>& body) const -> StateSnapshot {
  return {
    .state = m_state,
    .body = body.empty() ? m_body : body,
  };
}

void SessionCore::restoreSnapshot(const StateSnapshot& snapshot) {
  m_state = snapshot.state;
  m_body = snapshot.body;
  m_inputQueue.clear();
  m_hasLastObstacleSignature = false;
  m_lastObstacleSignature = 0;
  m_dynamicObstacleConfidenceTicks = 0;
  m_prevObstacleSnapshot.clear();
  m_currObstacleSnapshot.clear();
  m_hasObstacleSnapshots = false;
  m_recentSpawnPoints.clear();
  resetStallGuard();
}

void SessionCore::applyPowerUpResult(const PowerUpConsumptionResult& result) {
  if (result.shieldActivated) {
    m_state.shieldActive = true;
  }
  if (result.miniApplied) {
    m_body = applyMiniShrink(m_body, 3);
  }
  m_state.activeBuff = result.activeBuffAfter;
  m_state.buffTicksRemaining = result.buffTicksRemaining;
  m_state.buffTicksTotal = result.buffTicksTotal;
}

auto SessionCore::tickPowerUpCountdown() -> bool {
  if (m_state.powerUpPos == QPoint(-1, -1) || m_state.powerUpTicksRemaining <= 0) {
    return false;
  }
  m_state.powerUpTicksRemaining -= 1;
  if (m_state.powerUpTicksRemaining > 0) {
    return false;
  }
  m_state.powerUpPos = QPoint(-1, -1);
  m_state.powerUpType = 0;
  return true;
}

auto SessionCore::isOccupied(const QPoint& point) const -> bool {
  const bool inSnake =
    std::ranges::any_of(m_body, [&point](const QPoint& bodyPoint) { return bodyPoint == point; });
  if (inSnake) {
    return true;
  }
  return std::ranges::any_of(
    m_state.obstacles, [&point](const QPoint& obstaclePoint) { return obstaclePoint == point; });
}

} // namespace nenoserpent::core
