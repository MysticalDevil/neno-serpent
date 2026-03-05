#pragma once

#include <QByteArray>
#include <QHash>
#include <QString>

namespace nenoserpent::adapter::bot {

enum class BotMode {
  Safe,
  Balanced,
  Aggressive,
};

enum class BotBackendMode {
  Off,
  Rule,
  Ml,
  Search,
};

struct StrategyConfig {
  int openSpaceWeight = 3;
  int safeNeighborWeight = 12;
  int targetDistanceWeight = 8;
  int straightBonus = 4;
  int foodConsumeBonus = 10;
  int trapPenalty = 40;
  int lookaheadDepth = 2;
  int lookaheadWeight = 10;
  int powerTargetPriorityThreshold = 14;
  int powerTargetDistanceSlack = 2;

  int choiceCooldownTicks = 2;
  int stateActionCooldownTicks = 4;

  QHash<int, int> powerPriorityByType;
};

struct StrategyLoadResult {
  StrategyConfig config;
  bool loaded = false;
  QString profile;
  QString source;
  QString error;
};

[[nodiscard]] auto defaultStrategyConfig() -> const StrategyConfig&;
[[nodiscard]] auto powerPriority(const StrategyConfig& config, int type) -> int;
[[nodiscard]] auto modeName(BotMode mode) -> QString;
[[nodiscard]] auto nextMode(BotMode mode) -> BotMode;
void applyModeDefaults(StrategyConfig& config, BotMode mode);
[[nodiscard]] auto backendModeName(BotBackendMode mode) -> QString;
[[nodiscard]] auto nextBackendMode(BotBackendMode mode) -> BotBackendMode;
[[nodiscard]] auto currentBuildProfileName() -> QString;
[[nodiscard]] auto loadStrategyConfigFromJson(const QByteArray& json,
                                              const QString& profile,
                                              const QString& source = QStringLiteral("inline"))
  -> StrategyLoadResult;
[[nodiscard]] auto loadStrategyConfig(
  const QString& profile,
  const QString& overrideFilePath = QString(),
  const QString& resourcePath = QStringLiteral(":/adapter/bot/strategy_profiles.json"))
  -> StrategyLoadResult;

} // namespace nenoserpent::adapter::bot
