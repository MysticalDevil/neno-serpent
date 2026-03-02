#include "adapter/bot/config.h"

#include "power_up_id.h"

namespace nenoserpent::adapter::bot {

namespace {
auto buildDefaultPowerPriority() -> QHash<int, int> {
  return {
    {PowerUpId::Shield, 30},
    {PowerUpId::Laser, 25},
    {PowerUpId::Magnet, 20},
    {PowerUpId::Portal, 18},
    {PowerUpId::Double, 16},
    {PowerUpId::Rich, 14},
    {PowerUpId::Slow, 11},
    {PowerUpId::Ghost, 9},
    {PowerUpId::Mini, 5},
  };
}
} // namespace

auto defaultStrategyConfig() -> const StrategyConfig& {
  static const StrategyConfig kConfig{
    .openSpaceWeight = 3,
    .safeNeighborWeight = 12,
    .targetDistanceWeight = 8,
    .straightBonus = 4,
    .foodConsumeBonus = 10,
    .trapPenalty = 40,
    .powerTargetPriorityThreshold = 14,
    .powerTargetDistanceSlack = 2,
    .choiceCooldownTicks = 2,
    .stateActionCooldownTicks = 4,
    .powerPriorityByType = buildDefaultPowerPriority(),
  };
  return kConfig;
}

auto powerPriority(const StrategyConfig& config, const int type) -> int {
  return config.powerPriorityByType.value(type, 0);
}

} // namespace nenoserpent::adapter::bot
