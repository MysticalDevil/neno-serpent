#include "adapter/bot/snapshot_builder.h"

#include "core/buff/runtime.h"

namespace nenoserpent::adapter::bot {

auto buildSnapshot(const SnapshotBuilderInput& input) -> Snapshot {
  return {
    .head = input.head,
    .direction = input.direction,
    .food = input.food,
    .powerUpPos = input.powerUpPos,
    .powerUpType = input.powerUpType,
    .score = input.score,
    .levelIndex = input.levelIndex,
    .ghostActive = input.activeBuff == static_cast<int>(nenoserpent::core::BuffId::Ghost),
    .shieldActive = input.shieldActive,
    .portalActive = input.activeBuff == static_cast<int>(nenoserpent::core::BuffId::Portal),
    .laserActive = input.activeBuff == static_cast<int>(nenoserpent::core::BuffId::Laser),
    .boardWidth = input.boardWidth,
    .boardHeight = input.boardHeight,
    .obstacles = input.obstacles,
    .body = input.body,
  };
}

} // namespace nenoserpent::adapter::bot
