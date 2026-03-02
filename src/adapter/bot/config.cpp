#include "adapter/bot/config.h"

#include <algorithm>

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

#include "power_up_id.h"

namespace nenoserpent::adapter::bot {

namespace {

auto buildDefaultPowerPriority() -> QHash<int, int> {
  return {
    {PowerUpId::Shield, 30},
    {PowerUpId::Laser, 25},
    {PowerUpId::Magnet, 20},
    {PowerUpId::Portal, 18},
    {PowerUpId::Double, 16},
    {PowerUpId::Rich, 14},
    {PowerUpId::Slow, 11},
    {PowerUpId::Ghost, 9},
    {PowerUpId::Mini, 5},
  };
}

auto intOrDefault(const QJsonObject& object, const QString& key, const int fallback) -> int {
  const auto value = object.value(key);
  return value.isDouble() ? value.toInt() : fallback;
}

void applyOverrides(StrategyConfig& config, const QJsonObject& object) {
  config.openSpaceWeight =
    intOrDefault(object, QStringLiteral("openSpaceWeight"), config.openSpaceWeight);
  config.safeNeighborWeight =
    intOrDefault(object, QStringLiteral("safeNeighborWeight"), config.safeNeighborWeight);
  config.targetDistanceWeight =
    intOrDefault(object, QStringLiteral("targetDistanceWeight"), config.targetDistanceWeight);
  config.straightBonus =
    intOrDefault(object, QStringLiteral("straightBonus"), config.straightBonus);
  config.foodConsumeBonus =
    intOrDefault(object, QStringLiteral("foodConsumeBonus"), config.foodConsumeBonus);
  config.trapPenalty = intOrDefault(object, QStringLiteral("trapPenalty"), config.trapPenalty);
  config.lookaheadDepth =
    intOrDefault(object, QStringLiteral("lookaheadDepth"), config.lookaheadDepth);
  config.lookaheadWeight =
    intOrDefault(object, QStringLiteral("lookaheadWeight"), config.lookaheadWeight);
  config.powerTargetPriorityThreshold = intOrDefault(
    object, QStringLiteral("powerTargetPriorityThreshold"), config.powerTargetPriorityThreshold);
  config.powerTargetDistanceSlack = intOrDefault(
    object, QStringLiteral("powerTargetDistanceSlack"), config.powerTargetDistanceSlack);
  config.choiceCooldownTicks =
    intOrDefault(object, QStringLiteral("choiceCooldownTicks"), config.choiceCooldownTicks);
  config.stateActionCooldownTicks = intOrDefault(
    object, QStringLiteral("stateActionCooldownTicks"), config.stateActionCooldownTicks);

  const auto powerPriorityValue = object.value(QStringLiteral("powerPriorityByType"));
  if (!powerPriorityValue.isObject()) {
    return;
  }
  const auto priorityObject = powerPriorityValue.toObject();
  for (auto it = priorityObject.begin(); it != priorityObject.end(); ++it) {
    bool ok = false;
    const int type = it.key().toInt(&ok);
    if (!ok || !it.value().isDouble()) {
      continue;
    }
    config.powerPriorityByType.insert(type, it.value().toInt());
  }
}

auto loadJsonBytes(const QString& filePath, QByteArray& outBytes, QString& outError) -> bool {
  QFile file(filePath);
  if (!file.open(QIODevice::ReadOnly)) {
    outError = QStringLiteral("open failed: %1").arg(filePath);
    return false;
  }
  outBytes = file.readAll();
  return true;
}

} // namespace

auto defaultStrategyConfig() -> const StrategyConfig& {
  static const StrategyConfig kConfig{
    .openSpaceWeight = 3,
    .safeNeighborWeight = 12,
    .targetDistanceWeight = 8,
    .straightBonus = 4,
    .foodConsumeBonus = 10,
    .trapPenalty = 40,
    .lookaheadDepth = 2,
    .lookaheadWeight = 10,
    .powerTargetPriorityThreshold = 14,
    .powerTargetDistanceSlack = 2,
    .choiceCooldownTicks = 2,
    .stateActionCooldownTicks = 4,
    .powerPriorityByType = buildDefaultPowerPriority(),
  };
  return kConfig;
}

auto powerPriority(const StrategyConfig& config, const int type) -> int {
  return config.powerPriorityByType.value(type, 0);
}

auto modeName(const BotMode mode) -> QString {
  switch (mode) {
  case BotMode::Off:
    return QStringLiteral("off");
  case BotMode::Safe:
    return QStringLiteral("safe");
  case BotMode::Balanced:
    return QStringLiteral("balanced");
  case BotMode::Aggressive:
    return QStringLiteral("aggressive");
  }
  return QStringLiteral("off");
}

auto nextMode(const BotMode mode) -> BotMode {
  switch (mode) {
  case BotMode::Off:
    return BotMode::Safe;
  case BotMode::Safe:
    return BotMode::Balanced;
  case BotMode::Balanced:
    return BotMode::Aggressive;
  case BotMode::Aggressive:
    return BotMode::Off;
  }
  return BotMode::Off;
}

void applyModeDefaults(StrategyConfig& config, const BotMode mode) {
  switch (mode) {
  case BotMode::Off:
    break;
  case BotMode::Safe:
    config.safeNeighborWeight += 5;
    config.trapPenalty += 10;
    config.targetDistanceWeight = std::max(1, config.targetDistanceWeight - 2);
    config.lookaheadDepth = std::max(2, config.lookaheadDepth);
    break;
  case BotMode::Balanced:
    break;
  case BotMode::Aggressive:
    config.foodConsumeBonus += 8;
    config.straightBonus += 3;
    config.targetDistanceWeight += 2;
    config.safeNeighborWeight = std::max(0, config.safeNeighborWeight - 4);
    config.trapPenalty = std::max(0, config.trapPenalty - 8);
    break;
  }
}

auto currentBuildProfileName() -> QString {
#if defined(NENOSERPENT_BUILD_DEBUG)
  return QStringLiteral("debug");
#elif defined(NENOSERPENT_BUILD_DEV)
  return QStringLiteral("dev");
#else
  return QStringLiteral("release");
#endif
}

auto loadStrategyConfigFromJson(const QByteArray& json,
                                const QString& profile,
                                const QString& source) -> StrategyLoadResult {
  StrategyLoadResult result{
    .config = defaultStrategyConfig(),
    .loaded = false,
    .profile = profile,
    .source = source,
    .error = {},
  };

  QJsonParseError parseError{};
  const auto document = QJsonDocument::fromJson(json, &parseError);
  if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
    result.error = QStringLiteral("invalid json: %1").arg(parseError.errorString());
    return result;
  }

  const auto root = document.object();
  const auto profilesValue = root.value(QStringLiteral("profiles"));
  if (!profilesValue.isObject()) {
    result.error = QStringLiteral("missing object key: profiles");
    return result;
  }

  const auto profiles = profilesValue.toObject();
  const auto defaultProfile = profiles.value(QStringLiteral("default"));
  if (defaultProfile.isObject()) {
    applyOverrides(result.config, defaultProfile.toObject());
  }

  const auto selected = profiles.value(profile);
  if (!selected.isObject()) {
    result.error = QStringLiteral("missing profile: %1").arg(profile);
    return result;
  }
  applyOverrides(result.config, selected.toObject());
  result.loaded = true;
  return result;
}

auto loadStrategyConfig(const QString& profile,
                        const QString& overrideFilePath,
                        const QString& resourcePath) -> StrategyLoadResult {
  if (!overrideFilePath.trimmed().isEmpty()) {
    QByteArray jsonBytes;
    QString error;
    if (loadJsonBytes(overrideFilePath.trimmed(), jsonBytes, error)) {
      auto result = loadStrategyConfigFromJson(jsonBytes, profile, overrideFilePath.trimmed());
      if (result.loaded) {
        return result;
      }
      result.error = QStringLiteral("%1 (override)").arg(result.error);
      return result;
    }
    return {
      .config = defaultStrategyConfig(),
      .loaded = false,
      .profile = profile,
      .source = overrideFilePath.trimmed(),
      .error = error,
    };
  }

  QByteArray jsonBytes;
  QString error;
  if (!loadJsonBytes(resourcePath, jsonBytes, error)) {
    return {
      .config = defaultStrategyConfig(),
      .loaded = false,
      .profile = profile,
      .source = resourcePath,
      .error = error,
    };
  }
  return loadStrategyConfigFromJson(jsonBytes, profile, resourcePath);
}

} // namespace nenoserpent::adapter::bot
