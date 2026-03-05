#include "adapter/bot/applier.h"

namespace nenoserpent::adapter::bot {

auto applyDecision(const DecisionApplyInput& input) -> DecisionApplyResult {
  DecisionApplyResult result{};
  result.consumeTick = input.decision.consumeTick;

  if (input.decision.enqueueDirection.has_value()) {
    result.enqueueDirection = input.decision.enqueueDirection;
    result.enqueuedDirection =
      input.enqueueDirection ? input.enqueueDirection(*input.decision.enqueueDirection) : false;
  }

  if (input.decision.setChoiceIndex.has_value()) {
    result.choiceIndex = input.decision.setChoiceIndex;
    if (*input.decision.setChoiceIndex != input.currentChoiceIndex) {
      if (input.setChoiceIndex) {
        input.setChoiceIndex(*input.decision.setChoiceIndex);
      }
      result.choiceChanged = true;
    }
  }

  if (input.decision.triggerStart) {
    if (input.triggerStart) {
      input.triggerStart();
    }
    result.startTriggered = true;
  }

  return result;
}

} // namespace nenoserpent::adapter::bot
