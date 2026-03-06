#include "adapter/view_models/render.h"

#include "adapter/engine.h"

SessionRenderViewModel::SessionRenderViewModel(EngineAdapter* engineAdapter, QObject* parent)
    : QObject(parent),
      m_engineAdapter(engineAdapter) {
  if (m_engineAdapter == nullptr) {
    return;
  }

  connect(m_engineAdapter, &EngineAdapter::foodChanged, this, &SessionRenderViewModel::foodChanged);
  connect(
    m_engineAdapter, &EngineAdapter::powerUpChanged, this, &SessionRenderViewModel::powerUpChanged);
  connect(
    m_engineAdapter, &EngineAdapter::scoreChanged, this, &SessionRenderViewModel::scoreChanged);
  connect(m_engineAdapter,
          &EngineAdapter::highScoreChanged,
          this,
          &SessionRenderViewModel::highScoreChanged);
  connect(
    m_engineAdapter, &EngineAdapter::stateChanged, this, &SessionRenderViewModel::stateChanged);
  connect(m_engineAdapter,
          &EngineAdapter::obstaclesChanged,
          this,
          &SessionRenderViewModel::obstaclesChanged);
  connect(
    m_engineAdapter, &EngineAdapter::ghostChanged, this, &SessionRenderViewModel::ghostChanged);
  connect(m_engineAdapter, &EngineAdapter::buffChanged, this, &SessionRenderViewModel::buffChanged);
  connect(m_engineAdapter,
          &EngineAdapter::reflectionOffsetChanged,
          this,
          &SessionRenderViewModel::reflectionOffsetChanged);
}

auto SessionRenderViewModel::snakeModel() const -> SnakeModel* {
  return m_engineAdapter != nullptr ? m_engineAdapter->snakeModelPtr() : nullptr;
}

auto SessionRenderViewModel::food() const -> QPoint {
  return m_engineAdapter != nullptr ? m_engineAdapter->food() : QPoint{};
}

auto SessionRenderViewModel::powerUpPos() const -> QPoint {
  return m_engineAdapter != nullptr ? m_engineAdapter->powerUpPos() : QPoint{};
}

auto SessionRenderViewModel::powerUpType() const -> int {
  return m_engineAdapter != nullptr ? m_engineAdapter->powerUpType() : 0;
}

auto SessionRenderViewModel::score() const -> int {
  return m_engineAdapter != nullptr ? m_engineAdapter->score() : 0;
}

auto SessionRenderViewModel::highScore() const -> int {
  return m_engineAdapter != nullptr ? m_engineAdapter->highScore() : 0;
}

auto SessionRenderViewModel::state() const -> AppState::Value {
  return m_engineAdapter != nullptr ? m_engineAdapter->state() : AppState::Splash;
}

auto SessionRenderViewModel::boardWidth() const -> int {
  return m_engineAdapter != nullptr ? m_engineAdapter->boardWidth() : 0;
}

auto SessionRenderViewModel::boardHeight() const -> int {
  return m_engineAdapter != nullptr ? m_engineAdapter->boardHeight() : 0;
}

auto SessionRenderViewModel::obstacles() const -> QVariantList {
  return m_engineAdapter != nullptr ? m_engineAdapter->obstacles() : QVariantList{};
}

auto SessionRenderViewModel::ghost() const -> QVariantList {
  return m_engineAdapter != nullptr ? m_engineAdapter->ghost() : QVariantList{};
}

auto SessionRenderViewModel::activeBuff() const -> int {
  return m_engineAdapter != nullptr ? m_engineAdapter->activeBuff() : 0;
}

auto SessionRenderViewModel::buffTicksRemaining() const -> int {
  return m_engineAdapter != nullptr ? m_engineAdapter->buffTicksRemaining() : 0;
}

auto SessionRenderViewModel::buffTicksTotal() const -> int {
  return m_engineAdapter != nullptr ? m_engineAdapter->buffTicksTotal() : 0;
}

auto SessionRenderViewModel::reflectionOffset() const -> QPointF {
  return m_engineAdapter != nullptr ? m_engineAdapter->reflectionOffset() : QPointF{};
}

auto SessionRenderViewModel::shieldActive() const -> bool {
  return m_engineAdapter != nullptr ? m_engineAdapter->shieldActive() : false;
}

auto SessionRenderViewModel::scoutHintCell() const -> QPoint {
  return m_engineAdapter != nullptr ? m_engineAdapter->scoutHintCell() : QPoint(-1, -1);
}
