#include <QList>

#include "adapter/engine.h"
#include "adapter/profile/bridge.h"
#include "core/game/rules.h"

using namespace Qt::StringLiterals;

auto EngineAdapter::hasSave() const -> bool {
  return saveRepository().hasSession();
}

auto EngineAdapter::hasReplay() const noexcept -> bool {
  return !m_bestInputHistory.isEmpty();
}

void EngineAdapter::resetTransientRuntimeState() {
  m_sessionCore.resetTransientRuntimeState();
  m_choicePending = false;
  m_choiceIndex = 0;
  m_noFoodElapsedMs = 0;
}

void EngineAdapter::resetReplayRuntimeTracking() {
  m_ghostFrameIndex = 0;
  m_replayInputHistoryIndex = 0;
  m_replayChoiceHistoryIndex = 0;
  m_currentInputHistory.clear();
  m_currentRecording.clear();
  m_currentChoiceHistory.clear();
}

void EngineAdapter::nextLevel() {
  const int levelCount = m_levelRepository.levelCount();
  m_levelIndex = (m_levelIndex + 1) % levelCount;
  loadLevelData(m_levelIndex);
  if (m_state == AppState::StartMenu && hasSave()) {
    clearSavedState();
  }
  emit levelChanged();
  nenoserpent::adapter::setLevelIndex(m_profileManager.get(), m_levelIndex);
}
