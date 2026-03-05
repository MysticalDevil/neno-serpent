#include "adapter/bot/runtime.h"

#include <algorithm>

#include "core/game/rules.h"
#include "power_up_id.h"

namespace nenoserpent::adapter::bot {

namespace {

struct ResolvedBackend {
  const BotBackend* primary = nullptr;
  QString reason;
  bool usedFallback = false;
};

auto resolveBackend(const RuntimeInput& input) -> ResolvedBackend {
  if (input.backend != nullptr && input.backend->isAvailable()) {
    return {.primary = input.backend, .reason = {}, .usedFallback = false};
  }
  if (input.backend != nullptr && input.fallbackBackend != nullptr &&
      input.fallbackBackend->isAvailable()) {
    return {
      .primary = input.fallbackBackend,
      .reason = QStringLiteral("backend-unavailable"),
      .usedFallback = true,
    };
  }
  if (input.fallbackBackend != nullptr && input.fallbackBackend->isAvailable()) {
    return {
      .primary = input.fallbackBackend,
      .reason = QStringLiteral("primary-missing"),
      .usedFallback = true,
    };
  }
  if (input.backend != nullptr) {
    return {.primary = input.backend, .reason = {}, .usedFallback = false};
  }
  return {.primary = &ruleBackend(), .reason = {}, .usedFallback = false};
}

auto contextualChoiceStrategy(const StrategyConfig& base, const Snapshot& snapshot)
  -> StrategyConfig {
  StrategyConfig contextual = base;
  const DecisionPolicy policy = decisionPolicyFromEnvironment();
  const int boardCells = std::max(1, snapshot.boardWidth * snapshot.boardHeight);
  const int bodyPermille = (static_cast<int>(snapshot.body.size()) * 1000) / boardCells;
  const int obstaclePermille =
    (static_cast<int>(snapshot.obstacles.size()) * 1000) / boardCells;

  auto boostPriority = [&contextual](const int type, const int delta) {
    const int current = powerPriority(contextual, type);
    contextual.powerPriorityByType.insert(type, current + delta);
  };

  if (policy == DecisionPolicy::Conservative) {
    boostPriority(PowerUpId::Mini, 18);
    boostPriority(PowerUpId::Shield, 14);
    boostPriority(PowerUpId::Portal, 10);
    boostPriority(PowerUpId::Double, -8);
    boostPriority(PowerUpId::Rich, -10);
  } else if (policy == DecisionPolicy::Aggressive) {
    boostPriority(PowerUpId::Mini, -10);
    boostPriority(PowerUpId::Shield, -8);
    boostPriority(PowerUpId::Double, 14);
    boostPriority(PowerUpId::Rich, 18);
    boostPriority(PowerUpId::Magnet, 8);
  }

  if (bodyPermille >= 200 || snapshot.body.size() >= 18) {
    boostPriority(PowerUpId::Mini, 24);
    boostPriority(PowerUpId::Shield, 8);
  }
  if (obstaclePermille >= 90 || static_cast<int>(snapshot.obstacles.size()) >= 12) {
    boostPriority(PowerUpId::Laser, 32);
    boostPriority(PowerUpId::Portal, 22);
  }
  if (snapshot.score >= 120 && snapshot.body.size() <= 12) {
    boostPriority(PowerUpId::Double, 16);
    boostPriority(PowerUpId::Rich, 18);
  }
  return contextual;
}

auto dynamicChoiceCooldownTicks(const StrategyConfig& strategy, const Snapshot& snapshot) -> int {
  const DecisionPolicy policy = decisionPolicyFromEnvironment();
  const int intervalMs =
    std::clamp(nenoserpent::core::tickIntervalForScore(snapshot.score), 60, 200);
  const int fastness = 200 - intervalMs; // 0..140
  const int extraTicks = fastness / 28;  // 0..5
  int policyTicks = 0;
  if (policy == DecisionPolicy::Conservative) {
    policyTicks = 2;
  } else if (policy == DecisionPolicy::Aggressive) {
    policyTicks = -1;
  }
  return std::clamp(strategy.choiceCooldownTicks + extraTicks + policyTicks,
                    std::max(0, strategy.choiceCooldownTicks - 1),
                    strategy.choiceCooldownTicks + 8);
}

} // namespace

auto step(const RuntimeInput& input) -> RuntimeOutput {
  const StrategyConfig& strategy =
    (input.strategy != nullptr) ? *input.strategy : defaultStrategyConfig();
  const auto resolved = resolveBackend(input);
  const BotBackend& backend = *resolved.primary;
  RuntimeOutput output{};
  output.nextCooldownTicks = input.cooldownTicks;
  output.backend = backend.name();
  output.usedFallback = resolved.usedFallback;
  output.fallbackReason = resolved.reason;
  if (!input.enabled) {
    output.nextCooldownTicks = 0;
    return output;
  }

  if (output.nextCooldownTicks > 0) {
    --output.nextCooldownTicks;
  }

  if (input.state == AppState::Playing) {
    output.enqueueDirection = backend.decideDirection(input.snapshot, strategy);
    if (!output.enqueueDirection.has_value() && input.fallbackBackend != nullptr &&
        input.fallbackBackend != &backend && input.fallbackBackend->isAvailable()) {
      output.enqueueDirection = input.fallbackBackend->decideDirection(input.snapshot, strategy);
      if (output.enqueueDirection.has_value()) {
        output.backend = input.fallbackBackend->name();
        output.usedFallback = true;
        output.fallbackReason = QStringLiteral("direction-empty");
      }
    }
    return output;
  }

  if (output.nextCooldownTicks > 0) {
    return output;
  }

  if (input.state == AppState::ChoiceSelection) {
    const StrategyConfig choiceStrategy = contextualChoiceStrategy(strategy, input.snapshot);
    int bestIndex = backend.decideChoice(input.choices, choiceStrategy);
    if (bestIndex < 0 && input.fallbackBackend != nullptr && input.fallbackBackend != &backend &&
        input.fallbackBackend->isAvailable()) {
      bestIndex = input.fallbackBackend->decideChoice(input.choices, choiceStrategy);
      if (bestIndex >= 0) {
        output.backend = input.fallbackBackend->name();
        output.usedFallback = true;
        output.fallbackReason = QStringLiteral("choice-empty");
      }
    }
    if (bestIndex < 0) {
      return output;
    }
    output.setChoiceIndex = bestIndex;
    output.triggerStart = true;
    output.consumeTick = true;
    output.nextCooldownTicks = dynamicChoiceCooldownTicks(strategy, input.snapshot);
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
