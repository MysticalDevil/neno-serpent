#include "adapter/view_models/status.h"

#include "adapter/engine.h"

SessionStatusViewModel::SessionStatusViewModel(EngineAdapter* engineAdapter, QObject* parent)
    : QObject(parent),
      m_engineAdapter(engineAdapter) {
  if (m_engineAdapter == nullptr) {
    return;
  }

  connect(
    m_engineAdapter, &EngineAdapter::hasSaveChanged, this, &SessionStatusViewModel::hasSaveChanged);
  connect(m_engineAdapter,
          &EngineAdapter::highScoreChanged,
          this,
          &SessionStatusViewModel::hasReplayChanged);
  connect(m_engineAdapter,
          &EngineAdapter::highScoreChanged,
          this,
          &SessionStatusViewModel::highScoreChanged);
  connect(
    m_engineAdapter, &EngineAdapter::levelChanged, this, &SessionStatusViewModel::levelChanged);
}

auto SessionStatusViewModel::hasSave() const -> bool {
  return m_engineAdapter != nullptr ? m_engineAdapter->hasSave() : false;
}

auto SessionStatusViewModel::hasReplay() const -> bool {
  return m_engineAdapter != nullptr ? m_engineAdapter->hasReplay() : false;
}

auto SessionStatusViewModel::highScore() const -> int {
  return m_engineAdapter != nullptr ? m_engineAdapter->highScore() : 0;
}

auto SessionStatusViewModel::level() const -> int {
  return m_engineAdapter != nullptr ? m_engineAdapter->level() : 0;
}

auto SessionStatusViewModel::currentLevelName() const -> QString {
  return m_engineAdapter != nullptr ? m_engineAdapter->currentLevelName() : QString{};
}
