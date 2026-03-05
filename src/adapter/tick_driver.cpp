#include <cmath>

#include <QDateTime>

#include "adapter/bot/decision_applier.h"
#include "adapter/bot/orchestrator.h"
#include "adapter/bot/snapshot_builder.h"
#include "adapter/engine_adapter.h"
#include "fsm/game_state.h"
#include "logging/categories.h"

using namespace Qt::StringLiterals;

void EngineAdapter::updateReflectionFallback() {
  if (m_hasAccelerometerReading) {
    return;
  }
  const float t = static_cast<float>(QDateTime::currentMSecsSinceEpoch()) / 1000.0F;
  m_reflectionOffset = QPointF(std::sin(t * 0.8F) * 0.01F, std::cos(t * 0.7F) * 0.01F);
  emit reflectionOffsetChanged();
}

void EngineAdapter::update() {
  if (driveBotAutoplay()) {
    updateReflectionFallback();
    return;
  }

  if (m_fsmState) {
    dispatchStateCallback([](GameState& state) -> void { state.update(); });
    if (m_state == AppState::Playing || m_state == AppState::Replaying) {
      const auto runtimeUpdate = m_sessionCore.beginRuntimeUpdate();
      if (runtimeUpdate.buffExpired) {
        deactivateBuff();
      }
      if (runtimeUpdate.powerUpExpired) {
        emit powerUpChanged();
      }
      applyPostTickTasks();
    }
  }
  updateReflectionFallback();
}

auto EngineAdapter::driveBotAutoplay() -> bool {
  if (!m_fsmState) {
    return false;
  }
  const auto orchestratorOutput = nenoserpent::adapter::bot::runOrchestratorTick(
    m_botState,
    {
      .state = m_state,
      .snapshot = nenoserpent::adapter::bot::buildSnapshot({
        .head = m_sessionCore.headPosition(),
        .direction = m_sessionCore.direction(),
        .food = m_session.food,
        .powerUpPos = m_session.powerUpPos,
        .powerUpType = m_session.powerUpType,
        .score = m_session.score,
        .levelIndex = m_levelIndex,
        .activeBuff = m_session.activeBuff,
        .shieldActive = m_session.shieldActive,
        .boardWidth = BOARD_WIDTH,
        .boardHeight = BOARD_HEIGHT,
        .obstacles = m_session.obstacles,
        .body = m_sessionCore.body(),
      }),
      .choices = m_choices,
      .currentChoiceIndex = m_choiceIndex,
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

  const auto applyResult = nenoserpent::adapter::bot::applyDecision({
    .decision = decision,
    .currentChoiceIndex = m_choiceIndex,
    .enqueueDirection = [this](const QPoint& direction) -> bool {
      return m_sessionCore.enqueueDirection(direction);
    },
    .setChoiceIndex = [this](const int choiceIndex) -> void {
      m_choiceIndex = choiceIndex;
      emit choiceIndexChanged();
    },
    .triggerStart = [this]() -> void {
      dispatchStateCallback([](GameState& state) -> void { state.handleStart(); });
    },
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
      << "bot trigger start from state:" << static_cast<int>(m_state);
  }

  return applyResult.consumeTick;
}
