#include <cmath>

#include <QDateTime>

#include "adapter/engine.h"
#include "fsm/game_state.h"

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

  const int prevActiveBuff = m_session.activeBuff;
  const int prevBuffTicksRemaining = m_session.buffTicksRemaining;
  const int prevBuffTicksTotal = m_session.buffTicksTotal;
  const bool prevShieldActive = m_session.shieldActive;
  const QPoint prevScoutHintCell = m_session.scoutHintCell;

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
    if (m_state == AppState::Playing) {
      advanceChoiceSpeedRecovery();
    } else {
      cancelChoiceSpeedRecovery();
    }
  }
  if (prevActiveBuff != m_session.activeBuff ||
      prevBuffTicksRemaining != m_session.buffTicksRemaining ||
      prevBuffTicksTotal != m_session.buffTicksTotal ||
      prevShieldActive != m_session.shieldActive || prevScoutHintCell != m_session.scoutHintCell) {
    emit buffChanged();
  }
  updateReflectionFallback();
}

auto EngineAdapter::driveBotAutoplay() -> bool {
  if (!m_fsmState || m_botRuntimePort == nullptr) {
    return false;
  }
  return m_botRuntimePort->runTick(
    {
      .state = m_state,
      .snapshotInput =
        {
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
        },
      .choices = m_choices,
      .currentChoiceIndex = m_choiceIndex,
    },
    {
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
}
