#pragma once

#include <QString>

#include "adapter/bot/runtime.h"
#include "adapter/bot/state.h"

namespace nenoserpent::adapter::bot {

struct RouteTelemetry {
  bool changed = false;
  bool usedFallback = false;
  QString backend;
  QString fallbackReason;
};

[[nodiscard]] auto updateRouteTelemetry(State& state, const RuntimeOutput& decision)
  -> RouteTelemetry;

} // namespace nenoserpent::adapter::bot
