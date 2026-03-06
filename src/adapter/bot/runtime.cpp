#include "adapter/bot/runtime.h"

#include <algorithm>
#include <vector>

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
  const int obstaclePermille = (static_cast<int>(snapshot.obstacles.size()) * 1000) / boardCells;

  auto boostPriority = [&contextual](const int type, const int delta) {
    const int current = powerPriority(contextual, type);
    contextual.powerPriorityByType.insert(type, current + delta);
  };

  if (policy == DecisionPolicy::Conservative) {
    boostPriority(PowerUpId::Mini, 18);
    boostPriority(PowerUpId::Shield, 14);
    boostPriority(PowerUpId::Portal, 10);
    boostPriority(PowerUpId::Gold, -6);
    boostPriority(PowerUpId::Freeze, 10);
    boostPriority(PowerUpId::Anchor, 12);
  } else if (policy == DecisionPolicy::Aggressive) {
    boostPriority(PowerUpId::Mini, -10);
    boostPriority(PowerUpId::Shield, -8);
    boostPriority(PowerUpId::Gold, 14);
    boostPriority(PowerUpId::Magnet, 8);
    boostPriority(PowerUpId::Vacuum, 12);
  }

  if (bodyPermille >= 200 || snapshot.body.size() >= 18) {
    boostPriority(PowerUpId::Mini, 24);
    boostPriority(PowerUpId::Shield, 8);
  }
  if (obstaclePermille >= 90 || static_cast<int>(snapshot.obstacles.size()) >= 12) {
    boostPriority(PowerUpId::Laser, 32);
    boostPriority(PowerUpId::Portal, 22);
    boostPriority(PowerUpId::Freeze, 18);
  }
  if (snapshot.score >= 120 && snapshot.body.size() <= 12) {
    boostPriority(PowerUpId::Gold, 16);
    boostPriority(PowerUpId::Anchor, 10);
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

auto isMlLikeBackendName(const QString& backendName) -> bool {
  return backendName == QStringLiteral("ml") || backendName == QStringLiteral("ml-online");
}

struct DirectionBackendCandidate {
  const BotBackend* backend = nullptr;
  QString reason;
};

struct ForcedDirectionResult {
  std::optional<QPoint> direction;
  QString backend;
  QString decisionSummary;
};

auto appendDirectionCandidate(std::vector<DirectionBackendCandidate>& out,
                              const BotBackend* backend,
                              QString reason) -> void {
  if (backend == nullptr || !backend->isAvailable()) {
    return;
  }
  for (const auto& item : out) {
    if (item.backend == backend) {
      return;
    }
  }
  out.push_back({.backend = backend, .reason = std::move(reason)});
}

auto directionCandidates(const RuntimeInput& input, const ResolvedBackend& resolved)
  -> std::vector<DirectionBackendCandidate> {
  std::vector<DirectionBackendCandidate> out;
  out.reserve(3);
  appendDirectionCandidate(out, resolved.primary, resolved.reason);

  const bool primaryIsMlLike =
    resolved.primary != nullptr && isMlLikeBackendName(resolved.primary->name());
  if (primaryIsMlLike) {
    appendDirectionCandidate(out, &searchBackend(), QStringLiteral("direction-empty-search"));
    appendDirectionCandidate(out, input.fallbackBackend, QStringLiteral("direction-empty-rule"));
  } else {
    appendDirectionCandidate(out, input.fallbackBackend, QStringLiteral("direction-empty-rule"));
    appendDirectionCandidate(out, &searchBackend(), QStringLiteral("direction-empty-search"));
  }
  return out;
}

auto forcedCenterDirection(const RuntimeInput& input, const StrategyConfig& strategy)
  -> ForcedDirectionResult {
  ForcedDirectionResult result{};
  if (!input.forceCenterPush || input.state != AppState::Playing || input.snapshot.body.empty() ||
      input.snapshot.boardWidth <= 0 || input.snapshot.boardHeight <= 0) {
    return result;
  }

  Snapshot centerSnapshot = input.snapshot;
  centerSnapshot.food = QPoint(centerSnapshot.boardWidth / 2, centerSnapshot.boardHeight / 2);
  centerSnapshot.powerUpPos = QPoint(-1, -1);
  centerSnapshot.powerUpType = 0;

  StrategyConfig centerStrategy = strategy;
  centerStrategy.modeWeights.powerTargetPriorityThreshold = 100;
  centerStrategy.modeWeights.powerTargetDistanceSlack = 0;
  centerStrategy.modeWeights.targetDistanceWeight =
    std::min(80, centerStrategy.modeWeights.targetDistanceWeight + 14);
  centerStrategy.modeWeights.openSpaceWeight =
    std::min(60, centerStrategy.modeWeights.openSpaceWeight + 8);
  centerStrategy.modeWeights.safeNeighborWeight =
    std::min(80, centerStrategy.modeWeights.safeNeighborWeight + 8);
  centerStrategy.modeWeights.trapPenalty =
    std::min(120, centerStrategy.modeWeights.trapPenalty + 16);
  centerStrategy.modeWeights.lookaheadDepth =
    std::max(centerStrategy.modeWeights.lookaheadDepth, 3);
  centerStrategy.modeWeights.straightBonus =
    std::max(0, centerStrategy.modeWeights.straightBonus - 3);

  const auto& backend = searchBackend();
  result.direction = backend.decideDirection(centerSnapshot, centerStrategy);
  result.decisionSummary = backend.lastDecisionSummary();
  if (result.direction.has_value()) {
    result.backend = backend.name();
  }
  return result;
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
    const auto forced = forcedCenterDirection(input, strategy);
    if (forced.direction.has_value()) {
      output.enqueueDirection = forced.direction;
      output.decisionSummary = forced.decisionSummary;
      output.backend = forced.backend;
      output.usedFallback = true;
      output.fallbackReason = QStringLiteral("direction-empty-search-circuit");
      return output;
    }

    const auto candidates = directionCandidates(input, resolved);
    for (const auto& candidate : candidates) {
      output.enqueueDirection = candidate.backend->decideDirection(input.snapshot, strategy);
      output.decisionSummary = candidate.backend->lastDecisionSummary();
      if (!output.enqueueDirection.has_value()) {
        continue;
      }
      output.backend = candidate.backend->name();
      if (candidate.backend == &backend) {
        output.usedFallback = resolved.usedFallback;
        output.fallbackReason = resolved.reason;
      } else {
        output.usedFallback = true;
        output.fallbackReason = candidate.reason;
      }
      break;
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
