#include <QDateTime>

#include "adapter/engine.h"
#include "fsm/game_state.h"
#include "fsm/state_factory.h"
#include "power_up_id.h"

void EngineAdapter::restart() {
  resetTransientRuntimeState();
  m_sessionCore.applyMetaAction(nenoserpent::core::MetaAction::resetReplayRuntime());
  resetReplayRuntimeTracking();

  m_randomSeed = static_cast<uint>(QDateTime::currentMSecsSinceEpoch());
  m_rng.seed(m_randomSeed);

  loadLevelData(m_levelIndex);
  m_sessionCore.applyMetaAction(
    nenoserpent::core::MetaAction::bootstrapForLevel(m_session.obstacles, BOARD_WIDTH, BOARD_HEIGHT));
  syncSnakeModelFromCore();
  clearSavedState();

  m_timer->setInterval(initialGameplayIntervalMs());
  m_timer->start();
  spawnFood();

  emit buffChanged();
  emit powerUpChanged();
  emit scoreChanged();
  emit foodChanged();
  requestStateChange(AppState::Playing);
}

void EngineAdapter::startReplay() {
  if (m_bestInputHistory.isEmpty()) {
    return;
  }

  resetTransientRuntimeState();
  m_sessionCore.applyMetaAction(nenoserpent::core::MetaAction::resetReplayRuntime());
  resetReplayRuntimeTracking();

  loadLevelData(m_bestLevelIndex);
  m_sessionCore.applyMetaAction(
    nenoserpent::core::MetaAction::bootstrapForLevel(m_session.obstacles, BOARD_WIDTH, BOARD_HEIGHT));
  syncSnakeModelFromCore();
  m_rng.seed(m_bestRandomSeed);
  m_timer->setInterval(initialGameplayIntervalMs());
  m_timer->start();
  spawnFood();

  emit scoreChanged();
  emit foodChanged();
  emit ghostChanged();
  if (auto nextState = nenoserpent::fsm::createStateFor(*this, AppState::Replaying); nextState) {
    changeState(std::move(nextState));
  }
}

void EngineAdapter::enterReplayState() {
  setInternalState(AppState::Replaying);
  m_replayInputHistoryIndex = 0;
  m_replayChoiceHistoryIndex = 0;
}

void EngineAdapter::debugSeedReplayBuffPreview() {
  stopEngineTimer();
  resetTransientRuntimeState();
  loadLevelData(m_levelIndex);
  m_sessionCore.applyMetaAction(nenoserpent::core::MetaAction::seedPreviewState({
    .obstacles = m_session.obstacles,
    .body = {{10, 4}, {10, 5}, {10, 6}, {10, 7}},
    .food = QPoint(12, 7),
    .direction = QPoint(0, -1),
    .powerUpPos = QPoint(-1, -1),
    .powerUpType = 0,
    .score = 42,
    .tickCounter = 64,
    .activeBuff = PowerUpId::Shield,
    .buffTicksRemaining = 92,
    .buffTicksTotal = 120,
    .shieldActive = true,
  }));
  syncSnakeModelFromCore();

  emit scoreChanged();
  emit foodChanged();
  emit powerUpChanged();
  emit buffChanged();
  emit ghostChanged();

  setInternalState(AppState::Replaying);
}

void EngineAdapter::togglePause() {
  if (m_state == AppState::Playing) {
    requestStateChange(AppState::Paused);
  } else if (m_state == AppState::Paused) {
    requestStateChange(AppState::Playing);
  }
}

void EngineAdapter::lazyInitState() {
  if (!m_fsmState) {
    if (auto nextState = nenoserpent::fsm::createStateFor(*this, AppState::Splash); nextState) {
      changeState(std::move(nextState));
    }
  }
}
