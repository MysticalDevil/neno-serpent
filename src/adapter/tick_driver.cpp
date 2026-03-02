#include <cmath>

#include <QDateTime>

#include "adapter/bot/controller.h"
#include "adapter/engine_adapter.h"
#include "core/buff/runtime.h"
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
  if (!m_botAutoplayEnabled || !m_fsmState) {
    return false;
  }
  if (m_botActionCooldownTicks > 0) {
    --m_botActionCooldownTicks;
  }

  if (m_state == AppState::Playing) {
    const auto direction = nenoserpent::adapter::bot::pickDirection({
      .head = m_sessionCore.headPosition(),
      .direction = m_sessionCore.direction(),
      .food = m_session.food,
      .powerUpPos = m_session.powerUpPos,
      .powerUpType = m_session.powerUpType,
      .ghostActive = m_session.activeBuff == static_cast<int>(nenoserpent::core::BuffId::Ghost),
      .shieldActive = m_session.shieldActive,
      .portalActive = m_session.activeBuff == static_cast<int>(nenoserpent::core::BuffId::Portal),
      .laserActive = m_session.activeBuff == static_cast<int>(nenoserpent::core::BuffId::Laser),
      .boardWidth = BOARD_WIDTH,
      .boardHeight = BOARD_HEIGHT,
      .obstacles = m_session.obstacles,
      .body = m_sessionCore.body(),
    });
    if (direction.has_value() && m_sessionCore.enqueueDirection(*direction)) {
      qCDebug(nenoserpentInputLog).noquote()
        << "bot enqueue direction:" << direction->x() << direction->y();
    }
    return false;
  }

  if (m_state == AppState::ChoiceSelection) {
    if (m_botActionCooldownTicks > 0) {
      return false;
    }
    const int bestIndex = nenoserpent::adapter::bot::pickChoiceIndex(m_choices);
    if (bestIndex < 0) {
      return false;
    }

    if (m_choiceIndex != bestIndex) {
      m_choiceIndex = bestIndex;
      emit choiceIndexChanged();
    }

    qCDebug(nenoserpentInputLog).noquote() << "bot pick choice index:" << bestIndex;
    dispatchStateCallback([](GameState& state) -> void { state.handleStart(); });
    m_botActionCooldownTicks = 2;
    return true;
  }

  if (m_botActionCooldownTicks > 0) {
    return false;
  }

  if (m_state == AppState::Paused || m_state == AppState::GameOver) {
    qCDebug(nenoserpentInputLog).noquote()
      << "bot resume/restart from state:" << static_cast<int>(m_state);
    dispatchStateCallback([](GameState& state) -> void { state.handleStart(); });
    m_botActionCooldownTicks = 4;
    return true;
  }

  if (m_state == AppState::Replaying) {
    qCDebug(nenoserpentInputLog).noquote() << "bot leaves replay to start menu";
    dispatchStateCallback([](GameState& state) -> void { state.handleStart(); });
    m_botActionCooldownTicks = 4;
    return true;
  }

  return false;
}
