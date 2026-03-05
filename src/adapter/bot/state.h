#pragma once

#include <utility>

#include <QVariantMap>

#include "adapter/bot/backend.h"
#include "adapter/bot/config.h"
#include "adapter/bot/ml_backend.h"

namespace nenoserpent::adapter::bot {

class State final {
public:
  State() = default;

  void initializeFromEnvironment();

  [[nodiscard]] auto autoplayEnabled() const -> bool;
  [[nodiscard]] auto backendModeName() const -> QString;
  [[nodiscard]] auto strategyModeName() const -> QString;

  void cycleBackendMode();
  void cycleStrategyMode();
  void resetStrategyModeDefaults();
  auto setParam(const QString& key, int value) -> bool;

  [[nodiscard]] auto status() const -> QVariantMap;
  [[nodiscard]] auto currentBackend() const -> const BotBackend*;

  [[nodiscard]] auto strategyConfig() const -> const StrategyConfig& {
    return m_strategyConfig;
  }
  [[nodiscard]] auto mlBackend() -> MlBackend& {
    return m_mlBackend;
  }
  [[nodiscard]] auto mlBackend() const -> const MlBackend& {
    return m_mlBackend;
  }

  [[nodiscard]] auto actionCooldownTicks() const -> int {
    return m_actionCooldownTicks;
  }
  void setActionCooldownTicks(const int value) {
    m_actionCooldownTicks = value;
  }
  void resetActionCooldownTicks() {
    m_actionCooldownTicks = 0;
  }

  [[nodiscard]] auto lastBackendRoute() const -> const QString& {
    return m_lastBackendRoute;
  }
  void setLastBackendRoute(QString value) {
    m_lastBackendRoute = std::move(value);
  }
  void clearLastBackendRoute() {
    m_lastBackendRoute.clear();
  }

private:
  void applyModeDefaults();

  BotMode m_strategyMode = BotMode::Balanced;
  BotBackendMode m_backendMode = BotBackendMode::Off;
  MlBackend m_mlBackend;
  QString m_lastBackendRoute;
  int m_actionCooldownTicks = 0;
  StrategyConfig m_baseStrategyConfig = defaultStrategyConfig();
  StrategyConfig m_strategyConfig = m_baseStrategyConfig;
};

} // namespace nenoserpent::adapter::bot
