#pragma once

#include <optional>

#include <QVariantList>

#include "adapter/bot/config.h"
#include "adapter/bot/controller.h"
#include "app_state.h"

namespace nenoserpent::adapter::bot {

struct RuntimeInput {
  bool enabled = false;
  int cooldownTicks = 0;
  AppState::Value state = AppState::Splash;
  Snapshot snapshot;
  QVariantList choices;
  int currentChoiceIndex = 0;
  const StrategyConfig* strategy = nullptr;
};

struct RuntimeOutput {
  int nextCooldownTicks = 0;
  bool consumeTick = false;
  std::optional<QPoint> enqueueDirection;
  std::optional<int> setChoiceIndex;
  bool triggerStart = false;
};

[[nodiscard]] auto step(const RuntimeInput& input) -> RuntimeOutput;

} // namespace nenoserpent::adapter::bot
