#include "adapter/bot/runtime.h"

namespace nenoserpent::adapter::bot {

auto step(const RuntimeInput& input) -> RuntimeOutput {
  const StrategyConfig& strategy =
    (input.strategy != nullptr) ? *input.strategy : defaultStrategyConfig();
  const BotBackend& backend = (input.backend != nullptr) ? *input.backend : ruleBackend();
  RuntimeOutput output{};
  output.nextCooldownTicks = input.cooldownTicks;
  if (!input.enabled) {
    output.nextCooldownTicks = 0;
    return output;
  }

  if (output.nextCooldownTicks > 0) {
    --output.nextCooldownTicks;
  }

  if (input.state == AppState::Playing) {
    output.enqueueDirection = backend.decideDirection(input.snapshot, strategy);
    return output;
  }

  if (output.nextCooldownTicks > 0) {
    return output;
  }

  if (input.state == AppState::ChoiceSelection) {
    const int bestIndex = backend.decideChoice(input.choices, strategy);
    if (bestIndex < 0) {
      return output;
    }
    output.setChoiceIndex = bestIndex;
    output.triggerStart = true;
    output.consumeTick = true;
    output.nextCooldownTicks = strategy.choiceCooldownTicks;
    return output;
  }

  if (input.state == AppState::StartMenu || input.state == AppState::Paused ||
      input.state == AppState::GameOver || input.state == AppState::Replaying) {
    output.triggerStart = true;
    output.consumeTick = true;
    output.nextCooldownTicks = strategy.stateActionCooldownTicks;
    return output;
  }

  return output;
}

} // namespace nenoserpent::adapter::bot
