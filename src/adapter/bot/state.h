#pragma once

#include <cstdint>
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
  void onTick();

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
  auto observeDirectionEmptyRuleFallback(bool usedFallback, const QString& reason) -> bool;
  void observeDirectionFallback(bool usedFallback, const QString& reason);
  [[nodiscard]] auto forceCenterPushActive() const -> bool {
    return m_directionEmptySearchForceCenterTicks > 0;
  }
  void resetDirectionEmptyRuleStats();

private:
  void configureMlOnline(const QString& modelPath);
  void pollMlOnlineModelHotReload();
  void resetBackendRuntimeCaches();
  void applyModeDefaults();

  BotMode m_strategyMode = BotMode::Balanced;
  BotBackendMode m_backendMode = BotBackendMode::Off;
  MlBackend m_mlBackend;
  QString m_lastBackendRoute;
  int m_actionCooldownTicks = 0;
  StrategyConfig m_baseStrategyConfig = defaultStrategyConfig();
  StrategyConfig m_strategyConfig = m_baseStrategyConfig;
  QString m_mlModelPath;
  bool m_mlOnlineHotReloadEnabled = false;
  int m_mlOnlinePollIntervalTicks = 24;
  int m_mlOnlinePollCountdown = 24;
  std::int64_t m_mlModelLastModifiedMs = -1;
  int m_runtimeTicks = 0;
  int m_directionEmptyRuleTotal = 0;
  int m_directionEmptyRuleWindow = 0;
  int m_directionEmptyRuleWarnThreshold = 24;
  int m_directionEmptyRuleWarnInterval = 12;
  int m_directionEmptyRuleWindowTicks = 480;
  int m_directionEmptySearchConsecutive = 0;
  int m_directionEmptySearchFuseThreshold = 6;
  int m_directionEmptySearchForceCenterTicks = 0;
  int m_directionEmptySearchForceCenterDurationTicks = 36;
};

} // namespace nenoserpent::adapter::bot
