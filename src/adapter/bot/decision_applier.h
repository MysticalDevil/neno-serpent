#pragma once

#include <functional>
#include <optional>

#include "adapter/bot/runtime.h"

namespace nenoserpent::adapter::bot {

struct DecisionApplyInput {
  RuntimeOutput decision;
  int currentChoiceIndex = 0;
  std::function<bool(const QPoint&)> enqueueDirection;
  std::function<void(int)> setChoiceIndex;
  std::function<void()> triggerStart;
};

struct DecisionApplyResult {
  bool consumeTick = false;
  bool enqueuedDirection = false;
  bool choiceChanged = false;
  bool startTriggered = false;
  std::optional<QPoint> enqueueDirection;
  std::optional<int> choiceIndex;
};

auto applyDecision(const DecisionApplyInput& input) -> DecisionApplyResult;

} // namespace nenoserpent::adapter::bot
