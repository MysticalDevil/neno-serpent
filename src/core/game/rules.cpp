#include "core/game/rules.h"

#include <algorithm>
#include <cstdlib>

namespace nenoserpent::core {

auto roguelikeChoiceChancePercent(const RoguelikeChoiceContext& ctx) -> int {
  if (ctx.newScore < 8) {
    return 0;
  }
  // Prevent back-to-back prompts when score spikes from high-value buffs.
  if (ctx.newScore - ctx.lastChoiceScore < 6) {
    return 0;
  }
  // Safety net: guarantee one choice each 20-point milestone.
  if ((ctx.previousScore / 20) < (ctx.newScore / 20)) {
    return 100;
  }

  int chancePercent = 10;
  if (ctx.newScore >= 40) {
    chancePercent = 34;
  } else if (ctx.newScore >= 25) {
    chancePercent = 24;
  } else if (ctx.newScore >= 15) {
    chancePercent = 16;
  }

  // Crossing a 10-point band slightly boosts the roll.
  if ((ctx.previousScore / 10) < (ctx.newScore / 10)) {
    chancePercent += 8;
  }
  return std::min(chancePercent, 65);
}

auto tickIntervalForScore(const int score) -> int {
  // Slower early-game pace and gentler ramp:
  // - score=0   -> 230ms
  // - score=25  -> 215ms
  // - score=200 -> 105ms
  return std::max(72, 230 - ((score / 8) * 5));
}

auto wrapAxis(int value, int size) -> int {
  int wrapped = value % size;
  if (wrapped < 0) {
    wrapped += size;
  }
  return wrapped;
}

auto wrapPoint(const QPoint& point, int boardWidth, int boardHeight) -> QPoint {
  return {wrapAxis(point.x(), boardWidth), wrapAxis(point.y(), boardHeight)};
}

auto buildSafeInitialSnakeBody(const QList<QPoint>& obstacles, int boardWidth, int boardHeight)
  -> std::deque<QPoint> {
  auto blocked = [&obstacles](const QPoint& point) -> bool {
    for (const QPoint& obstaclePoint : obstacles) {
      if (obstaclePoint == point) {
        return true;
      }
    }
    return false;
  };

  QList<QPoint> candidates;
  candidates << QPoint(10, 10) << QPoint(10, 8) << QPoint(10, 6) << QPoint(5, 10) << QPoint(15, 10);
  for (int y = 0; y < boardHeight; ++y) {
    for (int x = 0; x < boardWidth; ++x) {
      candidates << QPoint(x, y);
    }
  }

  for (const QPoint& head : candidates) {
    const QPoint body1(head.x(), wrapAxis(head.y() + 1, boardHeight));
    const QPoint body2(head.x(), wrapAxis(head.y() + 2, boardHeight));
    if (!blocked(head) && !blocked(body1) && !blocked(body2)) {
      return {head, body1, body2};
    }
  }

  return {{10, 10}, {10, 11}, {10, 12}};
}

auto collectFreeSpots(int boardWidth,
                      int boardHeight,
                      const std::function<bool(const QPoint&)>& isBlocked) -> QList<QPoint> {
  QList<QPoint> freeSpots;
  for (int x = 0; x < boardWidth; ++x) {
    for (int y = 0; y < boardHeight; ++y) {
      const QPoint point(x, y);
      if (!isBlocked(point)) {
        freeSpots << point;
      }
    }
  }
  return freeSpots;
}

auto pickRandomFreeSpot(int boardWidth,
                        int boardHeight,
                        const std::function<bool(const QPoint&)>& isBlocked,
                        const std::function<int(int)>& pickIndex,
                        QPoint& pickedPoint) -> bool {
  const QList<QPoint> freeSpots = collectFreeSpots(boardWidth, boardHeight, isBlocked);
  if (freeSpots.isEmpty()) {
    return false;
  }
  const int selected = pickIndex(freeSpots.size());
  if (selected < 0 || selected >= freeSpots.size()) {
    return false;
  }
  pickedPoint = freeSpots[selected];
  return true;
}

auto magnetCandidateSpots(const QPoint& food, const QPoint& head, int boardWidth, int boardHeight)
  -> QList<QPoint> {
  auto axisStepToward = [](int from, int to, int size) -> int {
    if (from == to) {
      return 0;
    }
    const int forward = (to - from + size) % size;
    const int backward = (from - to + size) % size;
    return (forward <= backward) ? 1 : -1;
  };

  const int stepX = axisStepToward(food.x(), head.x(), boardWidth);
  const int stepY = axisStepToward(food.y(), head.y(), boardHeight);
  const bool preferX = std::abs(head.x() - food.x()) >= std::abs(head.y() - food.y());

  QList<QPoint> candidates;
  auto pushCandidate = [&](bool xAxis) -> void {
    if (xAxis) {
      if (stepX == 0) {
        return;
      }
      candidates.append(QPoint(wrapAxis(food.x() + stepX, boardWidth), food.y()));
    } else {
      if (stepY == 0) {
        return;
      }
      candidates.append(QPoint(food.x(), wrapAxis(food.y() + stepY, boardHeight)));
    }
  };

  if (preferX) {
    pushCandidate(true);
    pushCandidate(false);
  } else {
    pushCandidate(false);
    pushCandidate(true);
  }
  return candidates;
}

auto probeCollision(const QPoint& wrappedHead,
                    const QList<QPoint>& obstacles,
                    const std::deque<QPoint>& snakeBody,
                    bool ghostActive) -> CollisionProbe {
  CollisionProbe probe;
  for (int i = 0; i < obstacles.size(); ++i) {
    if (obstacles[i] == wrappedHead) {
      probe.hitsObstacle = true;
      probe.obstacleIndex = i;
      return probe;
    }
  }
  if (ghostActive) {
    return probe;
  }
  for (const QPoint& bodyPoint : snakeBody) {
    if (bodyPoint == wrappedHead) {
      probe.hitsBody = true;
      return probe;
    }
  }
  return probe;
}

auto collisionOutcomeForHead(const QPoint& head,
                             const int boardWidth,
                             const int boardHeight,
                             const QList<QPoint>& obstacles,
                             const std::deque<QPoint>& snakeBody,
                             const bool ghostActive,
                             const bool portalActive,
                             const bool laserActive,
                             const bool shieldActive) -> CollisionOutcome {
  const QPoint wrappedHead = wrapPoint(head, boardWidth, boardHeight);
  const CollisionProbe probe = probeCollision(wrappedHead, obstacles, snakeBody, ghostActive);
  CollisionOutcome outcome;
  if (probe.hitsObstacle) {
    if (portalActive) {
      return outcome;
    }
    if (laserActive) {
      outcome.consumeLaser = true;
      outcome.obstacleIndex = probe.obstacleIndex;
      return outcome;
    }
    if (shieldActive) {
      outcome.consumeShield = true;
      return outcome;
    }
    outcome.collision = true;
    return outcome;
  }
  if (probe.hitsBody) {
    if (shieldActive) {
      outcome.consumeShield = true;
      return outcome;
    }
    outcome.collision = true;
  }
  return outcome;
}

} // namespace nenoserpent::core
