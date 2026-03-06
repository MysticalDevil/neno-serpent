#pragma once

#include <optional>

#include "adapter/bot/config.h"

namespace nenoserpent::adapter::bot {

struct EnvironmentConfig {
  StrategyLoadResult strategyLoad;
  float minConfidence = 0.55F;
  float minMargin = 0.10F;
  bool minConfidenceValid = true;
  bool minMarginValid = true;
  QString mlModelPath;
  QString backendRaw;
  bool backendOverrideProvided = false;
  std::optional<BotBackendMode> backendOverride;
};

[[nodiscard]] auto loadEnvironmentConfig() -> EnvironmentConfig;

} // namespace nenoserpent::adapter::bot
