#include "adapter/bot/orchestrator.h"

#include "adapter/bot/route_telemetry.h"

namespace nenoserpent::adapter::bot {

auto runOrchestratorTick(State& state, const OrchestratorInput& input) -> OrchestratorOutput {
  const auto decision = step({
    .enabled = state.autoplayEnabled(),
    .cooldownTicks = state.actionCooldownTicks(),
    .state = input.state,
    .snapshot = input.snapshot,
    .choices = input.choices,
    .currentChoiceIndex = input.currentChoiceIndex,
    .strategy = &state.strategyConfig(),
    .backend = state.currentBackend(),
    .fallbackBackend = &ruleBackend(),
  });
  state.setActionCooldownTicks(decision.nextCooldownTicks);

  const auto routeTelemetry = updateRouteTelemetry(state, decision);

  return {
    .decision = decision,
    .routeTelemetry = routeTelemetry,
  };
}

} // namespace nenoserpent::adapter::bot
