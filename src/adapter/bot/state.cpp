#include "adapter/bot/state.h"

#include <algorithm>
#include <cstdint>

#include <QFileInfo>
#include <QVariantMap>

#include "adapter/bot/config_loader.h"
#include "logging/categories.h"

using namespace Qt::StringLiterals;

namespace nenoserpent::adapter::bot {

void State::initializeFromEnvironment() {
  const auto envConfig = loadEnvironmentConfig();
  const auto& strategyLoad = envConfig.strategyLoad;
  m_baseStrategyConfig = strategyLoad.config;
  m_strategyConfig = m_baseStrategyConfig;
  applyModeDefaults();
  if (strategyLoad.loaded) {
    qCInfo(nenoserpentInputLog).noquote()
      << "bot strategy loaded profile=" << strategyLoad.profile << "source=" << strategyLoad.source;
  } else {
    qCWarning(nenoserpentInputLog).noquote()
      << "bot strategy fallback to defaults profile=" << strategyLoad.profile
      << "source=" << strategyLoad.source << "reason=" << strategyLoad.error;
  }

  m_mlBackend.setConfidenceGate(envConfig.minConfidence, envConfig.minMargin);
  qCInfo(nenoserpentInputLog).noquote()
    << "bot ml gate conf=" << envConfig.minConfidence << "margin=" << envConfig.minMargin;
  if (!envConfig.minConfidenceValid || !envConfig.minMarginValid) {
    qCWarning(nenoserpentInputLog).noquote()
      << "invalid ml gate env override, using defaults when parse fails";
  }

  m_mlModelPath = envConfig.mlModelPath;
  configureMlOnline(m_mlModelPath);

  if (!m_mlModelPath.isEmpty()) {
    if (m_mlBackend.loadFromFile(m_mlModelPath)) {
      const QFileInfo modelInfo(m_mlModelPath);
      m_mlModelLastModifiedMs =
        modelInfo.exists() ? modelInfo.lastModified().toMSecsSinceEpoch() : -1;
      qCInfo(nenoserpentInputLog).noquote() << "bot ml model loaded source=" << m_mlModelPath;
    } else {
      qCWarning(nenoserpentInputLog).noquote()
        << "bot ml model unavailable source=" << m_mlModelPath
        << "reason=" << m_mlBackend.errorString();
    }
  } else {
    qCInfo(nenoserpentInputLog).noquote() << "bot ml model not configured";
  }

  if (!envConfig.backendOverrideProvided) {
    return;
  }
  if (envConfig.backendOverride.has_value()) {
    m_backendMode = *envConfig.backendOverride;
    qCInfo(nenoserpentInputLog).noquote() << "bot backend override ->" << envConfig.backendRaw;
    return;
  }
  qCWarning(nenoserpentInputLog).noquote()
    << "invalid bot backend override:" << envConfig.backendRaw
    << "(expected off|human|rule|ml|ml-online|search, fallback off)";
  m_backendMode = BotBackendMode::Off;
}

auto State::autoplayEnabled() const -> bool {
  return m_backendMode == BotBackendMode::Rule || m_backendMode == BotBackendMode::Ml ||
         m_backendMode == BotBackendMode::MlOnline || m_backendMode == BotBackendMode::Search;
}

auto State::backendModeName() const -> QString {
  return nenoserpent::adapter::bot::backendModeName(m_backendMode);
}

auto State::strategyModeName() const -> QString {
  return nenoserpent::adapter::bot::modeName(m_strategyMode);
}

void State::cycleBackendMode() {
  resetBackendRuntimeCaches();
  m_backendMode = nextBackendMode(m_backendMode);
  resetActionCooldownTicks();
  clearLastBackendRoute();
}

void State::cycleStrategyMode() {
  resetBackendRuntimeCaches();
  m_strategyMode = nextMode(m_strategyMode);
  resetActionCooldownTicks();
  applyModeDefaults();
}

void State::resetStrategyModeDefaults() {
  resetBackendRuntimeCaches();
  resetActionCooldownTicks();
  applyModeDefaults();
}

auto State::setParam(const QString& key, const int value) -> bool {
  const QString normalized = key.trimmed().toLower();
  if (normalized.isEmpty()) {
    return false;
  }
  auto clampRange = [](const int v, const int lo, const int hi) -> int {
    return std::max(lo, std::min(hi, v));
  };

  bool changed = false;
  if (normalized == u"openspaceweight"_s) {
    m_strategyConfig.openSpaceWeight = clampRange(value, 0, 60);
    changed = true;
  } else if (normalized == u"safeneighborweight"_s) {
    m_strategyConfig.safeNeighborWeight = clampRange(value, 0, 80);
    changed = true;
  } else if (normalized == u"targetdistanceweight"_s) {
    m_strategyConfig.targetDistanceWeight = clampRange(value, 0, 80);
    changed = true;
  } else if (normalized == u"straightbonus"_s) {
    m_strategyConfig.straightBonus = clampRange(value, 0, 40);
    changed = true;
  } else if (normalized == u"foodconsumebonus"_s) {
    m_strategyConfig.foodConsumeBonus = clampRange(value, 0, 80);
    changed = true;
  } else if (normalized == u"trappenalty"_s) {
    m_strategyConfig.trapPenalty = clampRange(value, 0, 120);
    changed = true;
  } else if (normalized == u"lookaheaddepth"_s) {
    m_strategyConfig.lookaheadDepth = clampRange(value, 0, 4);
    changed = true;
  } else if (normalized == u"lookaheadweight"_s) {
    m_strategyConfig.lookaheadWeight = clampRange(value, 0, 60);
    changed = true;
  } else if (normalized == u"powertargetprioritythreshold"_s) {
    m_strategyConfig.powerTargetPriorityThreshold = clampRange(value, 0, 100);
    changed = true;
  } else if (normalized == u"powertargetdistanceslack"_s) {
    m_strategyConfig.powerTargetDistanceSlack = clampRange(value, 0, 20);
    changed = true;
  } else if (normalized == u"tiebreakseed"_s) {
    m_strategyConfig.tieBreakSeed = value;
    changed = true;
  } else if (normalized == u"choicecooldownticks"_s) {
    m_strategyConfig.choiceCooldownTicks = clampRange(value, 0, 60);
    changed = true;
  } else if (normalized == u"stateactioncooldownticks"_s) {
    m_strategyConfig.stateActionCooldownTicks = clampRange(value, 0, 60);
    changed = true;
  }
  return changed;
}

auto State::status() const -> QVariantMap {
  return {
    {u"enabled"_s, autoplayEnabled()},
    {u"mode"_s, strategyModeName()},
    {u"backend"_s, backendModeName()},
    {u"mlAvailable"_s, m_mlBackend.isAvailable()},
    {u"mlOnlineHotReload"_s, m_mlOnlineHotReloadEnabled},
    {u"mlModelPath"_s, m_mlModelPath},
    {u"strategyMode"_s, strategyModeName()},
    {u"openSpaceWeight"_s, m_strategyConfig.openSpaceWeight},
    {u"safeNeighborWeight"_s, m_strategyConfig.safeNeighborWeight},
    {u"targetDistanceWeight"_s, m_strategyConfig.targetDistanceWeight},
    {u"straightBonus"_s, m_strategyConfig.straightBonus},
    {u"foodConsumeBonus"_s, m_strategyConfig.foodConsumeBonus},
    {u"trapPenalty"_s, m_strategyConfig.trapPenalty},
    {u"lookaheadDepth"_s, m_strategyConfig.lookaheadDepth},
    {u"lookaheadWeight"_s, m_strategyConfig.lookaheadWeight},
    {u"tieBreakSeed"_s, m_strategyConfig.tieBreakSeed},
    {u"choiceCooldownTicks"_s, m_strategyConfig.choiceCooldownTicks},
    {u"stateActionCooldownTicks"_s, m_strategyConfig.stateActionCooldownTicks},
  };
}

auto State::currentBackend() const -> const BotBackend* {
  switch (m_backendMode) {
  case BotBackendMode::Off:
  case BotBackendMode::Human:
    return nullptr;
  case BotBackendMode::Rule:
    return &ruleBackend();
  case BotBackendMode::Ml:
  case BotBackendMode::MlOnline:
    return &m_mlBackend;
  case BotBackendMode::Search:
    return &searchBackend();
  }
  return nullptr;
}

void State::onTick() {
  pollMlOnlineModelHotReload();
}

void State::configureMlOnline(const QString& modelPath) {
  bool hotReloadOk = false;
  const int hotReloadRaw =
    qEnvironmentVariableIntValue("NENOSERPENT_BOT_ML_ONLINE_HOT_RELOAD", &hotReloadOk);
  m_mlOnlineHotReloadEnabled = hotReloadOk ? hotReloadRaw != 0 : true;

  bool pollTicksOk = false;
  const int pollTicksRaw =
    qEnvironmentVariableIntValue("NENOSERPENT_BOT_ML_ONLINE_RELOAD_TICKS", &pollTicksOk);
  m_mlOnlinePollIntervalTicks = pollTicksOk ? std::max(1, pollTicksRaw) : 24;
  m_mlOnlinePollCountdown = m_mlOnlinePollIntervalTicks;
  if (m_mlOnlineHotReloadEnabled) {
    qCInfo(nenoserpentInputLog).noquote()
      << "bot ml-online hot-reload enabled pollTicks=" << m_mlOnlinePollIntervalTicks
      << "model=" << modelPath;
  }
}

void State::pollMlOnlineModelHotReload() {
  if (m_backendMode != BotBackendMode::MlOnline || !m_mlOnlineHotReloadEnabled ||
      m_mlModelPath.isEmpty()) {
    return;
  }
  --m_mlOnlinePollCountdown;
  if (m_mlOnlinePollCountdown > 0) {
    return;
  }
  m_mlOnlinePollCountdown = m_mlOnlinePollIntervalTicks;

  const QFileInfo modelInfo(m_mlModelPath);
  if (!modelInfo.exists()) {
    qCWarning(nenoserpentInputLog).noquote()
      << "bot ml-online model missing path=" << m_mlModelPath;
    return;
  }
  const std::int64_t lastModifiedMs = modelInfo.lastModified().toMSecsSinceEpoch();
  if (lastModifiedMs <= 0 || lastModifiedMs == m_mlModelLastModifiedMs) {
    return;
  }
  if (m_mlBackend.loadFromFile(m_mlModelPath)) {
    m_mlModelLastModifiedMs = lastModifiedMs;
    qCInfo(nenoserpentInputLog).noquote()
      << "bot ml-online model hot-reloaded source=" << m_mlModelPath;
    return;
  }
  qCWarning(nenoserpentInputLog).noquote()
    << "bot ml-online hot-reload failed source=" << m_mlModelPath
    << "reason=" << m_mlBackend.errorString();
}

void State::resetBackendRuntimeCaches() {
  m_mlBackend.reset();
  const_cast<BotBackend&>(ruleBackend()).reset();
  const_cast<BotBackend&>(searchBackend()).reset();
}

void State::applyModeDefaults() {
  m_strategyConfig = m_baseStrategyConfig;
  nenoserpent::adapter::bot::applyModeDefaults(m_strategyConfig, m_strategyMode);
}

} // namespace nenoserpent::adapter::bot
