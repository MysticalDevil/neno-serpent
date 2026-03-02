#include "adapter/bot/controller.h"

#include <algorithm>
#include <array>
#include <limits>
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

auto toroidalDistance(const QPoint& from, const QPoint& to, const int width, const int height)
  -> int {
  const int dx = std::abs(from.x() - to.x());
  const int dy = std::abs(from.y() - to.y());
  return std::min(dx, width - dx) + std::min(dy, height - dy);
}

auto boardIndex(const QPoint& p, const int width) -> int {
  return p.y() * width + p.x();
}

auto buildBlockedMap(const Snapshot& snapshot, const std::deque<QPoint>& projectedBody)
  -> std::vector<bool> {
  std::vector<bool> blocked(static_cast<std::size_t>(snapshot.boardWidth * snapshot.boardHeight),
                            false);

  if (!snapshot.portalActive && !snapshot.laserActive) {
    for (const QPoint& obstacle : snapshot.obstacles) {
      blocked[static_cast<std::size_t>(boardIndex(obstacle, snapshot.boardWidth))] = true;
    }
  }

  if (!snapshot.ghostActive) {
    for (const QPoint& bodyPart : projectedBody) {
      blocked[static_cast<std::size_t>(boardIndex(bodyPart, snapshot.boardWidth))] = true;
    }
  }

  return blocked;
}

auto floodReachable(const QPoint& start, const Snapshot& snapshot, const std::vector<bool>& blocked)
  -> int {
  const int width = snapshot.boardWidth;
  const int height = snapshot.boardHeight;
  if (width <= 0 || height <= 0) {
    return 0;
  }

  std::vector<bool> visited(blocked.size(), false);
  std::deque<QPoint> queue;
  queue.push_back(start);
  visited[static_cast<std::size_t>(boardIndex(start, width))] = true;
  int count = 0;

  while (!queue.empty()) {
    const QPoint current = queue.front();
    queue.pop_front();
    ++count;

    for (const QPoint& dir : kDirections) {
      const QPoint next = nenoserpent::core::wrapPoint(current + dir, width, height);
      const std::size_t nextIndex = static_cast<std::size_t>(boardIndex(next, width));
      if (visited[nextIndex] || blocked[nextIndex]) {
        continue;
      }
      visited[nextIndex] = true;
      queue.push_back(next);
    }
  }

  return count;
}

auto countSafeNeighbors(const QPoint& from,
                        const Snapshot& snapshot,
                        const std::vector<bool>& blocked) -> int {
  int safe = 0;
  for (const QPoint& dir : kDirections) {
    const QPoint next =
      nenoserpent::core::wrapPoint(from + dir, snapshot.boardWidth, snapshot.boardHeight);
    const std::size_t index = static_cast<std::size_t>(boardIndex(next, snapshot.boardWidth));
    if (!blocked[index]) {
      ++safe;
    }
  }
  return safe;
}

struct MovePreview {
  bool valid = false;
  QPoint wrappedHead{0, 0};
  std::deque<QPoint> nextBody;
};

auto previewMove(const Snapshot& snapshot,
                 const QPoint& head,
                 const QPoint& direction,
                 const std::deque<QPoint>& body,
                 const QPoint& candidate) -> MovePreview {
  MovePreview preview{};
  if (isReverseDirection(candidate, direction)) {
    return preview;
  }

  const QPoint nextHead = head + candidate;
  preview.wrappedHead =
    nenoserpent::core::wrapPoint(nextHead, snapshot.boardWidth, snapshot.boardHeight);
  std::deque<QPoint> collisionBody = body;
  const bool wouldEatFood = preview.wrappedHead == snapshot.food;
  if (!wouldEatFood && !collisionBody.empty()) {
    collisionBody.pop_back();
  }
  const auto collision = nenoserpent::core::collisionOutcomeForHead(nextHead,
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

  preview.nextBody = std::move(collisionBody);
  preview.nextBody.push_front(preview.wrappedHead);
  preview.valid = true;
  return preview;
}

auto countSafeContinuations(const Snapshot& snapshot,
                            const QPoint& head,
                            const QPoint& direction,
                            const std::deque<QPoint>& body,
                            const int depth) -> int {
  if (depth <= 0) {
    return 0;
  }

  int best = 0;
  for (const QPoint& candidate : kDirections) {
    const auto preview = previewMove(snapshot, head, direction, body, candidate);
    if (!preview.valid) {
      continue;
    }
    const int continuation =
      1 +
      countSafeContinuations(snapshot, preview.wrappedHead, candidate, preview.nextBody, depth - 1);
    if (continuation > best) {
      best = continuation;
    }
  }
  return best;
}

} // namespace

auto pickDirection(const Snapshot& snapshot, const StrategyConfig& config)
  -> std::optional<QPoint> {
  if (snapshot.body.empty() || snapshot.boardWidth <= 0 || snapshot.boardHeight <= 0) {
    return std::nullopt;
  }

  int bestScore = std::numeric_limits<int>::min();
  std::optional<QPoint> bestDirection;

  const bool hasPowerUp = snapshot.powerUpPos.x() >= 0 && snapshot.powerUpPos.y() >= 0;
  const int priority = powerPriority(config, snapshot.powerUpType);

  for (const QPoint& candidate : kDirections) {
    const auto preview =
      previewMove(snapshot, snapshot.head, snapshot.direction, snapshot.body, candidate);
    if (!preview.valid) {
      continue;
    }

    const QPoint wrappedHead = preview.wrappedHead;
    const bool wouldEatFood = wrappedHead == snapshot.food;
    const bool wouldEatPower = hasPowerUp && wrappedHead == snapshot.powerUpPos;

    auto blocked = buildBlockedMap(snapshot, preview.nextBody);
    const std::size_t headIndex =
      static_cast<std::size_t>(boardIndex(wrappedHead, snapshot.boardWidth));
    blocked[headIndex] = false;

    const int openSpace = floodReachable(wrappedHead, snapshot, blocked);
    const int safeNeighbors = countSafeNeighbors(wrappedHead, snapshot, blocked);

    QPoint target = snapshot.food;
    if (hasPowerUp && priority >= config.powerTargetPriorityThreshold) {
      const int foodDistance =
        toroidalDistance(wrappedHead, snapshot.food, snapshot.boardWidth, snapshot.boardHeight);
      const int powerDistance = toroidalDistance(
        wrappedHead, snapshot.powerUpPos, snapshot.boardWidth, snapshot.boardHeight);
      if (powerDistance <= foodDistance + config.powerTargetDistanceSlack) {
        target = snapshot.powerUpPos;
      }
    }

    const int distanceToTarget =
      toroidalDistance(wrappedHead, target, snapshot.boardWidth, snapshot.boardHeight);
    const int straightBonus = (candidate == snapshot.direction) ? config.straightBonus : 0;
    const int consumeBonus =
      (wouldEatFood ? config.foodConsumeBonus : 0) + (wouldEatPower ? priority : 0);
    const int trapPenalty = safeNeighbors <= 1 ? config.trapPenalty : 0;
    const int lookahead = countSafeContinuations(
      snapshot, wrappedHead, candidate, preview.nextBody, config.lookaheadDepth);

    const int score = (openSpace * config.openSpaceWeight) +
                      (safeNeighbors * config.safeNeighborWeight) + straightBonus + consumeBonus -
                      (distanceToTarget * config.targetDistanceWeight) - trapPenalty +
                      (lookahead * config.lookaheadWeight);
    if (score > bestScore) {
      bestScore = score;
      bestDirection = candidate;
    }
  }

  return bestDirection;
}

auto pickChoiceIndex(const QVariantList& choices, const StrategyConfig& config) -> int {
  int bestIndex = -1;
  int bestPriority = std::numeric_limits<int>::min();

  for (int i = 0; i < choices.size(); ++i) {
    const QVariantMap item = choices[i].toMap();
    if (item.isEmpty() || !item.contains(QStringLiteral("type"))) {
      continue;
    }
    const int type = item.value(QStringLiteral("type")).toInt();
    const int priority = powerPriority(config, type);
    if (priority > bestPriority) {
      bestPriority = priority;
      bestIndex = i;
    }
  }

  return bestIndex;
}

} // namespace nenoserpent::adapter::bot
