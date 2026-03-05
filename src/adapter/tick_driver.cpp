#include <cmath>

#include <QDateTime>

#include "adapter/bot/backend.h"
#include "adapter/bot/runtime.h"
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
  if (!m_fsmState) {
    return false;
  }
  const nenoserpent::adapter::bot::BotBackend* backend = nullptr;
  switch (m_botBackendMode) {
  case nenoserpent::adapter::bot::BotBackendMode::Off:
    break;
  case nenoserpent::adapter::bot::BotBackendMode::Rule:
    backend = &nenoserpent::adapter::bot::ruleBackend();
    break;
  case nenoserpent::adapter::bot::BotBackendMode::Ml:
    backend = &m_botMlBackend;
    break;
  case nenoserpent::adapter::bot::BotBackendMode::Search:
    backend = &nenoserpent::adapter::bot::searchBackend();
    break;
  }
  const auto decision = nenoserpent::adapter::bot::step({
    .enabled = botAutoplayEnabled(),
    .cooldownTicks = m_botActionCooldownTicks,
    .state = m_state,
    .snapshot =
      {
        .head = m_sessionCore.headPosition(),
        .direction = m_sessionCore.direction(),
        .food = m_session.food,
        .powerUpPos = m_session.powerUpPos,
        .powerUpType = m_session.powerUpType,
        .score = m_session.score,
        .levelIndex = m_levelIndex,
        .ghostActive = m_session.activeBuff == static_cast<int>(nenoserpent::core::BuffId::Ghost),
        .shieldActive = m_session.shieldActive,
        .portalActive = m_session.activeBuff == static_cast<int>(nenoserpent::core::BuffId::Portal),
        .laserActive = m_session.activeBuff == static_cast<int>(nenoserpent::core::BuffId::Laser),
        .boardWidth = BOARD_WIDTH,
        .boardHeight = BOARD_HEIGHT,
        .obstacles = m_session.obstacles,
        .body = m_sessionCore.body(),
      },
    .choices = m_choices,
    .currentChoiceIndex = m_choiceIndex,
    .strategy = &m_botStrategyConfig,
    .backend = backend,
    .fallbackBackend = &nenoserpent::adapter::bot::ruleBackend(),
  });
  m_botActionCooldownTicks = decision.nextCooldownTicks;

  if (botAutoplayEnabled()) {
    const QString routeKey = decision.usedFallback
                               ? decision.backend + u":"_s + decision.fallbackReason
                               : decision.backend;
    if (routeKey != m_lastBotBackendRoute) {
      m_lastBotBackendRoute = routeKey;
      if (decision.usedFallback) {
        qCInfo(nenoserpentInputLog).noquote()
          << "bot backend fallback ->" << decision.backend << "reason=" << decision.fallbackReason;
      } else {
        qCInfo(nenoserpentInputLog).noquote() << "bot backend route ->" << decision.backend;
      }
    }
  }

  if (decision.enqueueDirection.has_value() &&
      m_sessionCore.enqueueDirection(*decision.enqueueDirection)) {
    qCDebug(nenoserpentInputLog).noquote()
      << "bot enqueue direction:" << decision.enqueueDirection->x()
      << decision.enqueueDirection->y();
  }

  if (decision.setChoiceIndex.has_value() && m_choiceIndex != *decision.setChoiceIndex) {
    m_choiceIndex = *decision.setChoiceIndex;
    emit choiceIndexChanged();
    qCDebug(nenoserpentInputLog).noquote() << "bot pick choice index:" << *decision.setChoiceIndex;
  }

  if (decision.triggerStart) {
    qCDebug(nenoserpentInputLog).noquote()
      << "bot trigger start from state:" << static_cast<int>(m_state);
    dispatchStateCallback([](GameState& state) -> void { state.handleStart(); });
  }

  return decision.consumeTick;
}
