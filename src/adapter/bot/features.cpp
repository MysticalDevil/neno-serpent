#include "adapter/bot/features.h"

#include <deque>

#include "core/game/rules.h"

namespace nenoserpent::adapter::bot {

namespace {

auto isFatalCollisionOnMove(const Snapshot& snapshot, const QPoint& candidate) -> float {
  if (snapshot.boardWidth <= 0 || snapshot.boardHeight <= 0) {
    return 1.0F;
  }

  const QPoint nextHeadRaw = snapshot.head + candidate;
  const QPoint wrappedHead =
    nenoserpent::core::wrapPoint(nextHeadRaw, snapshot.boardWidth, snapshot.boardHeight);
  std::deque<QPoint> collisionBody = snapshot.body;
  const bool wouldEatFood = wrappedHead == snapshot.food;
  if (!wouldEatFood && !collisionBody.empty()) {
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
  return collision.collision ? 1.0F : 0.0F;
}

} // namespace

auto extractFeatures(const Snapshot& snapshot) -> Features {
  Features features{};
  const QPoint up(0, -1);
  const QPoint right(1, 0);
  const QPoint down(0, 1);
  const QPoint left(-1, 0);

  auto& v = features.values;
  v[0] = static_cast<float>(snapshot.levelIndex);
  v[1] = static_cast<float>(snapshot.score);
  v[2] = static_cast<float>(snapshot.body.size());
  v[3] = static_cast<float>(snapshot.head.x());
  v[4] = static_cast<float>(snapshot.head.y());
  v[5] = static_cast<float>(snapshot.direction.x());
  v[6] = static_cast<float>(snapshot.direction.y());
  v[7] = static_cast<float>(snapshot.food.x() - snapshot.head.x());
  v[8] = static_cast<float>(snapshot.food.y() - snapshot.head.y());
  v[9] = static_cast<float>(snapshot.powerUpPos.x() - snapshot.head.x());
  v[10] = static_cast<float>(snapshot.powerUpPos.y() - snapshot.head.y());
  v[11] = static_cast<float>(snapshot.powerUpType);
  v[12] = snapshot.powerUpType > 0 ? 1.0F : 0.0F;
  v[13] = snapshot.ghostActive ? 1.0F : 0.0F;
  v[14] = snapshot.shieldActive ? 1.0F : 0.0F;
  v[15] = snapshot.portalActive ? 1.0F : 0.0F;
  v[16] = snapshot.laserActive ? 1.0F : 0.0F;
  v[17] = isFatalCollisionOnMove(snapshot, up);
  v[18] = isFatalCollisionOnMove(snapshot, right);
  v[19] = isFatalCollisionOnMove(snapshot, down);
  v[20] = isFatalCollisionOnMove(snapshot, left);
  return features;
}

auto directionClass(const QPoint& direction) -> int {
  if (direction == QPoint(0, -1)) {
    return 0;
  }
  if (direction == QPoint(1, 0)) {
    return 1;
  }
  if (direction == QPoint(0, 1)) {
    return 2;
  }
  if (direction == QPoint(-1, 0)) {
    return 3;
  }
  return -1;
}

auto classDirection(const int actionClass) -> std::optional<QPoint> {
  switch (actionClass) {
  case 0:
    return QPoint(0, -1);
  case 1:
    return QPoint(1, 0);
  case 2:
    return QPoint(0, 1);
  case 3:
    return QPoint(-1, 0);
  default:
    return std::nullopt;
  }
}

} // namespace nenoserpent::adapter::bot
