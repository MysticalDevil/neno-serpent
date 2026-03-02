#pragma once

#include <deque>
#include <optional>

#include <QList>
#include <QPoint>
#include <QVariantList>

#include "adapter/bot/config.h"

namespace nenoserpent::adapter::bot {

struct Snapshot {
  QPoint head{0, 0};
  QPoint direction{0, -1};
  QPoint food{0, 0};
  QPoint powerUpPos{-1, -1};
  int powerUpType = 0;
  bool ghostActive = false;
  bool shieldActive = false;
  bool portalActive = false;
  bool laserActive = false;
  int boardWidth = 20;
  int boardHeight = 18;
  QList<QPoint> obstacles;
  std::deque<QPoint> body;
};

[[nodiscard]] auto pickDirection(const Snapshot& snapshot,
                                 const StrategyConfig& config = defaultStrategyConfig())
  -> std::optional<QPoint>;
[[nodiscard]] auto pickChoiceIndex(const QVariantList& choices,
                                   const StrategyConfig& config = defaultStrategyConfig()) -> int;

} // namespace nenoserpent::adapter::bot
