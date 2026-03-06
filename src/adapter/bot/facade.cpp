#include "adapter/bot/facade.h"

#include "adapter/bot/applier.h"
#include "adapter/bot/orchestrator.h"
#include "logging/categories.h"

namespace nenoserpent::adapter::bot {

void RuntimeFacade::initializeFromEnvironment() {
  m_state.initializeFromEnvironment();
}

auto RuntimeFacade::autoplayEnabled() const -> bool {
  return m_state.autoplayEnabled();
}

auto RuntimeFacade::modeName() const -> QString {
  return m_state.backendModeName();
}

auto RuntimeFacade::status() const -> QVariantMap {
  return m_state.status();
}

void RuntimeFacade::cycleBackendMode() {
  m_state.cycleBackendMode();
}

void RuntimeFacade::cycleStrategyMode() {
  m_state.cycleStrategyMode();
}

void RuntimeFacade::resetStrategyModeDefaults() {
  m_state.resetStrategyModeDefaults();
}

auto RuntimeFacade::setParam(const QString& key, const int value) -> bool {
  return m_state.setParam(key, value);
}

auto RuntimeFacade::runTick(const RuntimeTickInput& input, const RuntimeTickCallbacks& callbacks)
  -> bool {
  m_state.onTick();
  const auto orchestratorOutput =
    runOrchestratorTick(m_state,
                        {
                          .state = input.state,
                          .snapshot = buildSnapshot(input.snapshotInput),
                          .choices = input.choices,
                          .currentChoiceIndex = input.currentChoiceIndex,
                        });
  const auto& decision = orchestratorOutput.decision;
  const auto& routeTelemetry = orchestratorOutput.routeTelemetry;

  if (routeTelemetry.changed) {
    if (routeTelemetry.usedFallback) {
      qCInfo(nenoserpentInputLog).noquote() << "bot backend fallback ->" << routeTelemetry.backend
                                            << "reason=" << routeTelemetry.fallbackReason;
    } else {
      qCInfo(nenoserpentInputLog).noquote() << "bot backend route ->" << routeTelemetry.backend;
    }
  }
  if (!decision.decisionSummary.isEmpty()) {
    qCDebug(nenoserpentInputLog).noquote() << decision.decisionSummary;
  }
  if (m_state.observeDirectionEmptyRuleFallback(decision.usedFallback, decision.fallbackReason)) {
    const QVariantMap stats = m_state.status();
    qCWarning(nenoserpentInputLog).noquote()
      << "bot fallback alert: direction-empty-rule threshold reached"
      << "window=" << stats.value(QStringLiteral("directionEmptyRuleWindow")).toInt()
      << "total=" << stats.value(QStringLiteral("directionEmptyRuleTotal")).toInt()
      << "threshold=" << stats.value(QStringLiteral("directionEmptyRuleWarnThreshold")).toInt();
  }

  const auto applyResult = applyDecision({
    .decision = decision,
    .currentChoiceIndex = input.currentChoiceIndex,
    .enqueueDirection = callbacks.enqueueDirection,
    .setChoiceIndex = callbacks.setChoiceIndex,
    .triggerStart = callbacks.triggerStart,
  });

  if (applyResult.enqueuedDirection && applyResult.enqueueDirection.has_value()) {
    qCDebug(nenoserpentInputLog).noquote()
      << "bot enqueue direction:" << applyResult.enqueueDirection->x()
      << applyResult.enqueueDirection->y();
  }
  if (applyResult.choiceChanged && applyResult.choiceIndex.has_value()) {
    qCDebug(nenoserpentInputLog).noquote() << "bot pick choice index:" << *applyResult.choiceIndex;
  }
  if (applyResult.startTriggered) {
    qCDebug(nenoserpentInputLog).noquote()
      << "bot trigger start from state:" << static_cast<int>(input.state);
  }
  return applyResult.consumeTick;
}

} // namespace nenoserpent::adapter::bot
