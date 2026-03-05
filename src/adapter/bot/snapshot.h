#pragma once

#include <deque>

#include <QList>
#include <QPoint>

#include "adapter/bot/controller.h"

namespace nenoserpent::adapter::bot {

struct SnapshotBuilderInput {
  QPoint head{0, 0};
  QPoint direction{0, -1};
  QPoint food{0, 0};
  QPoint powerUpPos{-1, -1};
  int powerUpType = 0;
  int score = 0;
  int levelIndex = 0;
  int activeBuff = 0;
  bool shieldActive = false;
  int boardWidth = 20;
  int boardHeight = 18;
  QList<QPoint> obstacles;
  std::deque<QPoint> body;
};

[[nodiscard]] auto buildSnapshot(const SnapshotBuilderInput& input) -> Snapshot;

} // namespace nenoserpent::adapter::bot
