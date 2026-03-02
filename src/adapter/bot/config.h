#pragma once

#include <QHash>

namespace nenoserpent::adapter::bot {

struct StrategyConfig {
  int openSpaceWeight = 3;
  int safeNeighborWeight = 12;
  int targetDistanceWeight = 8;
  int straightBonus = 4;
  int foodConsumeBonus = 10;
  int trapPenalty = 40;
  int powerTargetPriorityThreshold = 14;
  int powerTargetDistanceSlack = 2;

  int choiceCooldownTicks = 2;
  int stateActionCooldownTicks = 4;

  QHash<int, int> powerPriorityByType;
};

[[nodiscard]] auto defaultStrategyConfig() -> const StrategyConfig&;
[[nodiscard]] auto powerPriority(const StrategyConfig& config, int type) -> int;

} // namespace nenoserpent::adapter::bot
