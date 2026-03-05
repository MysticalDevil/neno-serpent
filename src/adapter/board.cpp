#include <algorithm>

#include "adapter/engine.h"
#include "core/buff/runtime.h"
#include "core/game/rules.h"

void EngineAdapter::spawnFood() {
  if (m_sessionCore.spawnFood(
        BOARD_WIDTH, BOARD_HEIGHT, [this](const int size) { return m_rng.bounded(size); })) {
    emit foodChanged();
  }
}

void EngineAdapter::spawnPowerUp() {
  if (m_sessionCore.spawnPowerUp(
        BOARD_WIDTH, BOARD_HEIGHT, [this](const int size) { return m_rng.bounded(size); })) {
    emit powerUpChanged();
  }
}

auto EngineAdapter::isOutOfBounds(const QPoint& p) noexcept -> bool {
  return !m_boardRect.contains(p);
}
