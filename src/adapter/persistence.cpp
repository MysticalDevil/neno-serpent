#include "adapter/engine.h"
#include "adapter/profile/bridge.h"
#include "fsm/game_state.h"
#include "logging/categories.h"

using namespace Qt::StringLiterals;

auto EngineAdapter::saveRepository() const -> nenoserpent::services::SaveRepository {
  return nenoserpent::services::SaveRepository(m_profileManager.get());
}

void EngineAdapter::loadLastSession() {
  const auto snapshot = saveRepository().loadSessionSnapshot();
  if (!snapshot.has_value()) {
    return;
  }

  resetReplayRuntimeTracking();
  m_sessionCore.applyMetaAction(nenoserpent::core::MetaAction::restorePersistedSession(
    nenoserpent::adapter::toCoreStateSnapshot(*snapshot)));
  syncSnakeModelFromCore();

  for (const auto& p : snapshot->body) {
    m_currentRecording.append(p);
  }

  m_timer->setInterval(gameplayTickIntervalMs());
  m_timer->start();

  emit scoreChanged();
  emit foodChanged();
  emit obstaclesChanged();
  emit ghostChanged();
  requestStateChange(AppState::Paused);
}

void EngineAdapter::updatePersistence() {
  updateHighScore();
  nenoserpent::adapter::incrementCrashes(m_profileManager.get());
  checkAchievements();
  clearSavedState();
}

void EngineAdapter::enterGameOverState() {
  setInternalState(AppState::GameOver);
  updatePersistence();
}

void EngineAdapter::lazyInit() {
  m_levelIndex = nenoserpent::adapter::levelIndex(m_profileManager.get());
  emit audioSetVolume(nenoserpent::adapter::volume(m_profileManager.get()));

  nenoserpent::adapter::GhostSnapshot snapshot;
  if (saveRepository().loadGhostSnapshot(snapshot)) {
    m_bestRecording = snapshot.recording;
    m_bestRandomSeed = snapshot.randomSeed;
    m_bestInputHistory = snapshot.inputHistory;
    m_bestLevelIndex = snapshot.levelIndex;
    m_bestChoiceHistory = snapshot.choiceHistory;
  }

  loadLevelData(m_levelIndex);
  spawnFood();
  emit paletteChanged();
  emit shellColorChanged();
}

void EngineAdapter::updateHighScore() {
  if (m_session.score > nenoserpent::adapter::highScore(m_profileManager.get())) {
    nenoserpent::adapter::updateHighScore(m_profileManager.get(), m_session.score);
    m_bestInputHistory = m_currentInputHistory;
    m_bestRecording = m_currentRecording;
    m_bestChoiceHistory = m_currentChoiceHistory;
    m_bestRandomSeed = m_randomSeed;
    m_bestLevelIndex = m_levelIndex;

    const nenoserpent::adapter::GhostSnapshot ghostSnapshot{
      .recording = m_bestRecording,
      .randomSeed = m_bestRandomSeed,
      .inputHistory = m_bestInputHistory,
      .levelIndex = m_bestLevelIndex,
      .choiceHistory = m_bestChoiceHistory,
    };
    const bool savedGhost = saveRepository().saveGhostSnapshot(ghostSnapshot);
    if (!savedGhost) {
      qCWarning(nenoserpentReplayLog).noquote() << "failed to persist ghost snapshot";
    }
    emit highScoreChanged();
  }
}

void EngineAdapter::saveCurrentState() {
  if (m_profileManager) {
    saveRepository().saveSession(m_sessionCore.snapshot({}));
    emit hasSaveChanged();
  }
}

void EngineAdapter::clearSavedState() {
  if (m_profileManager) {
    saveRepository().clearSession();
    emit hasSaveChanged();
  }
}
