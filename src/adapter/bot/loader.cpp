#include "adapter/bot/loader.h"

#include <optional>
#include <utility>

namespace nenoserpent::adapter::bot {

namespace {

auto parseFloatOrDefault(const QString& text, const float fallback) -> std::pair<float, bool> {
  if (text.isEmpty()) {
    return {fallback, true};
  }
  bool ok = false;
  const float value = text.toFloat(&ok);
  return {ok ? value : fallback, ok};
}

auto parseBackendModeOverride(const QString& value) -> std::optional<BotBackendMode> {
  if (value == QStringLiteral("off")) {
    return BotBackendMode::Off;
  }
  if (value == QStringLiteral("human")) {
    return BotBackendMode::Human;
  }
  if (value == QStringLiteral("rule")) {
    return BotBackendMode::Rule;
  }
  if (value == QStringLiteral("ml")) {
    return BotBackendMode::Ml;
  }
  if (value == QStringLiteral("ml-online")) {
    return BotBackendMode::MlOnline;
  }
  if (value == QStringLiteral("search")) {
    return BotBackendMode::Search;
  }
  return std::nullopt;
}

} // namespace

auto loadEnvironmentConfig() -> EnvironmentConfig {
  const QString botStrategyOverride = qEnvironmentVariable("NENOSERPENT_BOT_STRATEGY_FILE");
  const auto strategyLoad = loadStrategyConfig(currentBuildProfileName(), botStrategyOverride);

  const QString minConfidenceRaw = qEnvironmentVariable("NENOSERPENT_BOT_ML_MIN_CONF").trimmed();
  const QString minMarginRaw = qEnvironmentVariable("NENOSERPENT_BOT_ML_MIN_MARGIN").trimmed();
  const auto [minConfidence, confOk] = parseFloatOrDefault(minConfidenceRaw, 0.55F);
  const auto [minMargin, marginOk] = parseFloatOrDefault(minMarginRaw, 0.10F);

  const QString backendOverrideRaw =
    qEnvironmentVariable("NENOSERPENT_BOT_BACKEND").trimmed().toLower();
  return {
    .strategyLoad = strategyLoad,
    .minConfidence = minConfidence,
    .minMargin = minMargin,
    .minConfidenceValid = confOk,
    .minMarginValid = marginOk,
    .mlModelPath = qEnvironmentVariable("NENOSERPENT_BOT_ML_MODEL").trimmed(),
    .backendRaw = backendOverrideRaw,
    .backendOverrideProvided = !backendOverrideRaw.isEmpty(),
    .backendOverride = parseBackendModeOverride(backendOverrideRaw),
  };
}

} // namespace nenoserpent::adapter::bot
