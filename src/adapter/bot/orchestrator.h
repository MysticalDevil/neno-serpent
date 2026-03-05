#pragma once

#include <QVariantList>

#include "adapter/bot/telemetry.h"
#include "adapter/bot/runtime.h"
#include "adapter/bot/state.h"
#include "app_state.h"

namespace nenoserpent::adapter::bot {

struct OrchestratorInput {
  AppState::Value state = AppState::Splash;
  Snapshot snapshot;
  QVariantList choices;
  int currentChoiceIndex = 0;
};

struct OrchestratorOutput {
  RuntimeOutput decision;
  RouteTelemetry routeTelemetry;
};

auto runOrchestratorTick(State& state, const OrchestratorInput& input) -> OrchestratorOutput;

} // namespace nenoserpent::adapter::bot
