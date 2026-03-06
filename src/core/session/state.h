#pragma once

#include <QList>
#include <QPoint>

namespace nenoserpent::core {

struct SessionState {
  QPoint food = {0, 0};
  QPoint powerUpPos = {-1, -1};
  int powerUpType = 0;
  int powerUpTicksRemaining = 0;
  int activeBuff = 0;
  int buffTicksRemaining = 0;
  int buffTicksTotal = 0;
  bool shieldActive = false;
  QPoint scoutHintCell = {-1, -1};
  QPoint direction = {0, -1};
  int score = 0;
  QList<QPoint> obstacles;
  int tickCounter = 0;
  int lastRoguelikeChoiceScore = -1000;
  int speedDownSteps = 0;
  int anchorTickIntervalMs = 0;
};

} // namespace nenoserpent::core
