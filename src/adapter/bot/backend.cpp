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

  QPoint target = snapshot.food;
  const bool hasPowerUp = snapshot.powerUpPos.x() >= 0 && snapshot.powerUpPos.y() >= 0;
  const int powerPriorityValue = powerPriority(config, snapshot.powerUpType);
  if (hasPowerUp && powerPriorityValue >= config.powerTargetPriorityThreshold) {
    const int foodDistance =
      toroidalDistance(state.head, snapshot.food, snapshot.boardWidth, snapshot.boardHeight);
    const int powerDistance =
      toroidalDistance(state.head, snapshot.powerUpPos, snapshot.boardWidth, snapshot.boardHeight);
    if (powerDistance <= foodDistance + config.powerTargetDistanceSlack) {
      target = snapshot.powerUpPos;
    }
  }
  const int distanceToTarget =
    toroidalDistance(state.head, target, snapshot.boardWidth, snapshot.boardHeight);
  const int trapPenalty = safeNeighbors <= 1 ? config.trapPenalty : 0;
  return (openSpace * config.openSpaceWeight) + (safeNeighbors * config.safeNeighborWeight) -
         (distanceToTarget * config.targetDistanceWeight) - trapPenalty + (state.score * 48);
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
              (revisitCount * 220);
  if (preview.ateFood) {
    score += config.foodConsumeBonus * 6;
  }
  if (preview.atePower) {
    score += powerPriority(config, snapshot.powerUpType) * 4;
  }
  return score;
}

auto selectLoopAwareDirection(const Snapshot& snapshot,
                              const StrategyConfig& config,
                              LoopMemory& memory,
                              const bool useSearchScoring) -> std::optional<QPoint> {
  if (snapshot.body.empty() || snapshot.boardWidth <= 0 || snapshot.boardHeight <= 0) {
    return std::nullopt;
  }
  const MoveState initial{
    .head = snapshot.head,
    .direction = snapshot.direction,
    .body = snapshot.body,
    .score = snapshot.score,
  };

  constexpr int kLoopRepeatThreshold = 4;
  const int repeats = memory.observe(snapshot, initial);
  const bool escapeMode = repeats >= kLoopRepeatThreshold;
  const int depth = std::clamp(config.lookaheadDepth + 1, 2, 6);
  const auto tieRotateSeed = static_cast<std::uint64_t>(stateHash(snapshot, initial)) ^
                             static_cast<std::uint64_t>(config.tieBreakSeed) ^ memory.observeTick();
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
    int score = 0;
    if (escapeMode) {
      score = evaluateEscapeCandidate(snapshot, preview, config, revisitCount);
    } else if (useSearchScoring) {
      int immediate = (candidate == snapshot.direction ? config.straightBonus : 0);
      if (preview.ateFood) {
        immediate += config.foodConsumeBonus;
      }
      if (preview.atePower) {
        immediate += powerPriority(config, snapshot.powerUpType);
      }
      score =
        immediate + searchValue(snapshot, preview.next, config, depth - 1) - (revisitCount * 48);
    } else {
      auto blocked = buildBlockedMap(snapshot, preview.next.body);
      if (const auto headIndex =
            tryBoardIndex(preview.next.head, snapshot.boardWidth, snapshot.boardHeight);
          headIndex.has_value()) {
        blocked[*headIndex] = false;
      }
      const int openSpace = floodReachable(preview.next.head, snapshot, blocked);
      const int safeNeighbors = countSafeNeighbors(preview.next.head, snapshot, blocked);
      const QPoint target =
        (snapshot.powerUpPos.x() >= 0 && snapshot.powerUpPos.y() >= 0 &&
         powerPriority(config, snapshot.powerUpType) >= config.powerTargetPriorityThreshold)
          ? snapshot.powerUpPos
          : snapshot.food;
      const int targetDistance =
        toroidalDistance(preview.next.head, target, snapshot.boardWidth, snapshot.boardHeight);
      int immediate = (candidate == snapshot.direction ? config.straightBonus : 0);
      if (preview.ateFood) {
        immediate += config.foodConsumeBonus;
      }
      if (preview.atePower) {
        immediate += powerPriority(config, snapshot.powerUpType);
      }
      const int trapPenalty = safeNeighbors <= 1 ? config.trapPenalty : 0;
      score = (openSpace * config.openSpaceWeight) + (safeNeighbors * config.safeNeighborWeight) -
              (targetDistance * config.targetDistanceWeight) + immediate - trapPenalty -
              (revisitCount * 56);
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
