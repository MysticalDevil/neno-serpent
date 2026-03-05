#include "adapter/bot/backend.h"

#include <algorithm>
#include <array>
#include <limits>
#include <optional>
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

class RuleBackend final : public BotBackend {
public:
  [[nodiscard]] auto name() const -> QString override {
    return QStringLiteral("rule");
  }

  [[nodiscard]] auto decideDirection(const Snapshot& snapshot, const StrategyConfig& config) const
    -> std::optional<QPoint> override {
    return pickDirection(snapshot, config);
  }

  [[nodiscard]] auto decideChoice(const QVariantList& choices, const StrategyConfig& config) const
    -> int override {
    return pickChoiceIndex(choices, config);
  }
};

class SearchBackend final : public BotBackend {
public:
  [[nodiscard]] auto name() const -> QString override {
    return QStringLiteral("search");
  }

  [[nodiscard]] auto decideDirection(const Snapshot& snapshot, const StrategyConfig& config) const
    -> std::optional<QPoint> override {
    if (snapshot.body.empty() || snapshot.boardWidth <= 0 || snapshot.boardHeight <= 0) {
      return std::nullopt;
    }

    const MoveState initial{
      .head = snapshot.head,
      .direction = snapshot.direction,
      .body = snapshot.body,
      .score = snapshot.score,
    };
    const int depth = std::clamp(config.lookaheadDepth + 1, 2, 6);

    int bestScore = std::numeric_limits<int>::min();
    std::optional<QPoint> bestDirection;
    for (const QPoint& candidate : kDirections) {
      const auto preview = previewMove(snapshot, initial, candidate);
      if (!preview.valid) {
        continue;
      }

      int immediate = (candidate == snapshot.direction ? config.straightBonus : 0);
      if (preview.ateFood) {
        immediate += config.foodConsumeBonus;
      }
      if (preview.atePower) {
        immediate += powerPriority(config, snapshot.powerUpType);
      }
      const int score = immediate + searchValue(snapshot, preview.next, config, depth - 1);
      if (score > bestScore) {
        bestScore = score;
        bestDirection = candidate;
      }
    }

    return bestDirection;
  }

  [[nodiscard]] auto decideChoice(const QVariantList& choices, const StrategyConfig& config) const
    -> int override {
    return pickChoiceIndex(choices, config);
  }
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
