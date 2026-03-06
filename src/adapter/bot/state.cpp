#include "adapter/bot/state.h"

#include <algorithm>
#include <cstdint>

#include <QFileInfo>
#include <QVariantMap>

#include "adapter/bot/loader.h"
#include "logging/categories.h"

using namespace Qt::StringLiterals;

namespace nenoserpent::adapter::bot {

void State::initializeFromEnvironment() {
  const auto envConfig = loadEnvironmentConfig();
  const auto& strategyLoad = envConfig.strategyLoad;
  m_baseStrategyConfig = strategyLoad.config;
  m_strategyConfig = m_baseStrategyConfig;
  applyModeDefaults();
  m_directionEmptySearchForceCenterDurationTicks = m_strategyConfig.recovery.centerRecoverTicks;
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

  bool warnThresholdOk = false;
  const int warnThresholdRaw = qEnvironmentVariableIntValue(
    "NENOSERPENT_BOT_DIRECTION_EMPTY_RULE_WARN_THRESHOLD", &warnThresholdOk);
  m_directionEmptyRuleWarnThreshold =
    warnThresholdOk ? std::max(1, warnThresholdRaw) : m_directionEmptyRuleWarnThreshold;

  bool warnIntervalOk = false;
  const int warnIntervalRaw = qEnvironmentVariableIntValue(
    "NENOSERPENT_BOT_DIRECTION_EMPTY_RULE_WARN_INTERVAL", &warnIntervalOk);
  m_directionEmptyRuleWarnInterval =
    warnIntervalOk ? std::max(1, warnIntervalRaw) : m_directionEmptyRuleWarnInterval;

  bool windowTicksOk = false;
  const int windowTicksRaw = qEnvironmentVariableIntValue(
    "NENOSERPENT_BOT_DIRECTION_EMPTY_RULE_WINDOW_TICKS", &windowTicksOk);
  m_directionEmptyRuleWindowTicks =
    windowTicksOk ? std::max(60, windowTicksRaw) : m_directionEmptyRuleWindowTicks;

  bool searchFuseThresholdOk = false;
  const int searchFuseThresholdRaw = qEnvironmentVariableIntValue(
    "NENOSERPENT_BOT_DIRECTION_EMPTY_SEARCH_FUSE_THRESHOLD", &searchFuseThresholdOk);
  m_directionEmptySearchFuseThreshold = searchFuseThresholdOk ? std::max(1, searchFuseThresholdRaw)
                                                              : m_directionEmptySearchFuseThreshold;

  bool searchFuseDurationOk = false;
  const int searchFuseDurationRaw = qEnvironmentVariableIntValue(
    "NENOSERPENT_BOT_DIRECTION_EMPTY_SEARCH_FUSE_FORCE_CENTER_TICKS", &searchFuseDurationOk);
  m_directionEmptySearchForceCenterDurationTicks =
    searchFuseDurationOk ? std::max(1, searchFuseDurationRaw)
                         : m_directionEmptySearchForceCenterDurationTicks;
  resetDirectionEmptyRuleStats();

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
  resetDirectionEmptyRuleStats();
}

void State::cycleStrategyMode() {
  resetBackendRuntimeCaches();
  m_strategyMode = nextMode(m_strategyMode);
  resetActionCooldownTicks();
  applyModeDefaults();
  resetDirectionEmptyRuleStats();
}

void State::resetStrategyModeDefaults() {
  resetBackendRuntimeCaches();
  resetActionCooldownTicks();
  applyModeDefaults();
  resetDirectionEmptyRuleStats();
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
  if (normalized == u"openspaceweight"_s || normalized == u"modeweights.openspaceweight"_s) {
    m_strategyConfig.modeWeights.openSpaceWeight = clampRange(value, 0, 60);
    m_strategyConfig.openSpaceWeight = m_strategyConfig.modeWeights.openSpaceWeight;
    changed = true;
  } else if (normalized == u"safeneighborweight"_s ||
             normalized == u"modeweights.safeneighborweight"_s) {
    m_strategyConfig.modeWeights.safeNeighborWeight = clampRange(value, 0, 80);
    m_strategyConfig.safeNeighborWeight = m_strategyConfig.modeWeights.safeNeighborWeight;
    changed = true;
  } else if (normalized == u"targetdistanceweight"_s ||
             normalized == u"modeweights.targetdistanceweight"_s) {
    m_strategyConfig.modeWeights.targetDistanceWeight = clampRange(value, 0, 80);
    m_strategyConfig.targetDistanceWeight = m_strategyConfig.modeWeights.targetDistanceWeight;
    changed = true;
  } else if (normalized == u"straightbonus"_s || normalized == u"modeweights.straightbonus"_s) {
    m_strategyConfig.modeWeights.straightBonus = clampRange(value, 0, 40);
    m_strategyConfig.straightBonus = m_strategyConfig.modeWeights.straightBonus;
    changed = true;
  } else if (normalized == u"foodconsumebonus"_s ||
             normalized == u"modeweights.foodconsumebonus"_s) {
    m_strategyConfig.modeWeights.foodConsumeBonus = clampRange(value, 0, 80);
    m_strategyConfig.foodConsumeBonus = m_strategyConfig.modeWeights.foodConsumeBonus;
    changed = true;
  } else if (normalized == u"trappenalty"_s || normalized == u"modeweights.trappenalty"_s) {
    m_strategyConfig.modeWeights.trapPenalty = clampRange(value, 0, 120);
    m_strategyConfig.trapPenalty = m_strategyConfig.modeWeights.trapPenalty;
    changed = true;
  } else if (normalized == u"lookaheaddepth"_s || normalized == u"modeweights.lookaheaddepth"_s) {
    m_strategyConfig.modeWeights.lookaheadDepth = clampRange(value, 0, 6);
    m_strategyConfig.lookaheadDepth = m_strategyConfig.modeWeights.lookaheadDepth;
    changed = true;
  } else if (normalized == u"lookaheadweight"_s || normalized == u"modeweights.lookaheadweight"_s) {
    m_strategyConfig.modeWeights.lookaheadWeight = clampRange(value, 0, 80);
    m_strategyConfig.lookaheadWeight = m_strategyConfig.modeWeights.lookaheadWeight;
    changed = true;
  } else if (normalized == u"powertargetprioritythreshold"_s ||
             normalized == u"modeweights.powertargetprioritythreshold"_s) {
    m_strategyConfig.modeWeights.powerTargetPriorityThreshold = clampRange(value, 0, 100);
    m_strategyConfig.powerTargetPriorityThreshold =
      m_strategyConfig.modeWeights.powerTargetPriorityThreshold;
    changed = true;
  } else if (normalized == u"powertargetdistanceslack"_s ||
             normalized == u"modeweights.powertargetdistanceslack"_s) {
    m_strategyConfig.modeWeights.powerTargetDistanceSlack = clampRange(value, 0, 30);
    m_strategyConfig.powerTargetDistanceSlack =
      m_strategyConfig.modeWeights.powerTargetDistanceSlack;
    changed = true;
  } else if (normalized == u"looprepeatpenalty"_s ||
             normalized == u"loopguard.looprepeatpenalty"_s) {
    m_strategyConfig.loopGuard.loopRepeatPenalty = clampRange(value, 0, 320);
    m_strategyConfig.loopRepeatPenalty = m_strategyConfig.loopGuard.loopRepeatPenalty;
    changed = true;
  } else if (normalized == u"loopescapepenalty"_s ||
             normalized == u"loopguard.loopescapepenalty"_s) {
    m_strategyConfig.loopGuard.loopEscapePenalty = clampRange(value, 0, 480);
    m_strategyConfig.loopEscapePenalty = m_strategyConfig.loopGuard.loopEscapePenalty;
    changed = true;
  } else if (normalized == u"loopguard.repeatstreakpenalty"_s) {
    m_strategyConfig.loopGuard.repeatStreakPenalty = clampRange(value, 0, 120);
    changed = true;
  } else if (normalized == u"loopguard.cycle4penalty"_s) {
    m_strategyConfig.loopGuard.cycle4Penalty = clampRange(value, 0, 600);
    changed = true;
  } else if (normalized == u"loopguard.cycle6penalty"_s) {
    m_strategyConfig.loopGuard.cycle6Penalty = clampRange(value, 0, 700);
    changed = true;
  } else if (normalized == u"loopguard.cycle8penalty"_s) {
    m_strategyConfig.loopGuard.cycle8Penalty = clampRange(value, 0, 800);
    changed = true;
  } else if (normalized == u"loopguard.taboopenalty"_s) {
    m_strategyConfig.loopGuard.tabooPenalty = clampRange(value, 0, 700);
    changed = true;
  } else if (normalized == u"loopguard.tabooescapeticks"_s) {
    m_strategyConfig.loopGuard.tabooEscapeTicks = clampRange(value, 0, 30);
    changed = true;
  } else if (normalized == u"loopguard.taboonormalticks"_s) {
    m_strategyConfig.loopGuard.tabooNormalTicks = clampRange(value, 0, 30);
    changed = true;
  } else if (normalized == u"recovery.noprogresspenaltybase"_s) {
    m_strategyConfig.recovery.noProgressPenaltyBase = clampRange(value, 0, 80);
    changed = true;
  } else if (normalized == u"recovery.noprogresspenaltyscale"_s) {
    m_strategyConfig.recovery.noProgressPenaltyScale = clampRange(value, 1, 8);
    changed = true;
  } else if (normalized == u"recovery.centerbiasweight"_s) {
    m_strategyConfig.recovery.centerBiasWeight = clampRange(value, 0, 120);
    changed = true;
  } else if (normalized == u"recovery.cornerleavebonus"_s) {
    m_strategyConfig.recovery.cornerLeaveBonus = clampRange(value, 0, 300);
    changed = true;
  } else if (normalized == u"recovery.cornerstickpenalty"_s) {
    m_strategyConfig.recovery.cornerStickPenalty = clampRange(value, 0, 300);
    changed = true;
  } else if (normalized == u"recovery.escaperatiosoftcappermille"_s) {
    m_strategyConfig.recovery.escapeRatioSoftCapPermille = clampRange(value, 300, 980);
    changed = true;
  } else if (normalized == u"recovery.centerrecoverticks"_s) {
    m_strategyConfig.recovery.centerRecoverTicks = clampRange(value, 1, 240);
    m_directionEmptySearchForceCenterDurationTicks = m_strategyConfig.recovery.centerRecoverTicks;
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
    {u"loopRepeatPenalty"_s, m_strategyConfig.loopRepeatPenalty},
    {u"loopEscapePenalty"_s, m_strategyConfig.loopEscapePenalty},
    {u"modeWeights.openSpaceWeight"_s, m_strategyConfig.modeWeights.openSpaceWeight},
    {u"modeWeights.safeNeighborWeight"_s, m_strategyConfig.modeWeights.safeNeighborWeight},
    {u"modeWeights.targetDistanceWeight"_s, m_strategyConfig.modeWeights.targetDistanceWeight},
    {u"modeWeights.straightBonus"_s, m_strategyConfig.modeWeights.straightBonus},
    {u"modeWeights.foodConsumeBonus"_s, m_strategyConfig.modeWeights.foodConsumeBonus},
    {u"modeWeights.trapPenalty"_s, m_strategyConfig.modeWeights.trapPenalty},
    {u"modeWeights.lookaheadDepth"_s, m_strategyConfig.modeWeights.lookaheadDepth},
    {u"modeWeights.lookaheadWeight"_s, m_strategyConfig.modeWeights.lookaheadWeight},
    {u"loopGuard.loopRepeatPenalty"_s, m_strategyConfig.loopGuard.loopRepeatPenalty},
    {u"loopGuard.loopEscapePenalty"_s, m_strategyConfig.loopGuard.loopEscapePenalty},
    {u"loopGuard.repeatStreakPenalty"_s, m_strategyConfig.loopGuard.repeatStreakPenalty},
    {u"loopGuard.cycle4Penalty"_s, m_strategyConfig.loopGuard.cycle4Penalty},
    {u"loopGuard.cycle6Penalty"_s, m_strategyConfig.loopGuard.cycle6Penalty},
    {u"loopGuard.cycle8Penalty"_s, m_strategyConfig.loopGuard.cycle8Penalty},
    {u"loopGuard.tabooPenalty"_s, m_strategyConfig.loopGuard.tabooPenalty},
    {u"loopGuard.tabooEscapeTicks"_s, m_strategyConfig.loopGuard.tabooEscapeTicks},
    {u"loopGuard.tabooNormalTicks"_s, m_strategyConfig.loopGuard.tabooNormalTicks},
    {u"recovery.noProgressPenaltyBase"_s, m_strategyConfig.recovery.noProgressPenaltyBase},
    {u"recovery.noProgressPenaltyScale"_s, m_strategyConfig.recovery.noProgressPenaltyScale},
    {u"recovery.centerBiasWeight"_s, m_strategyConfig.recovery.centerBiasWeight},
    {u"recovery.cornerLeaveBonus"_s, m_strategyConfig.recovery.cornerLeaveBonus},
    {u"recovery.cornerStickPenalty"_s, m_strategyConfig.recovery.cornerStickPenalty},
    {u"recovery.escapeRatioSoftCapPermille"_s,
     m_strategyConfig.recovery.escapeRatioSoftCapPermille},
    {u"recovery.centerRecoverTicks"_s, m_strategyConfig.recovery.centerRecoverTicks},
    {u"choiceCooldownTicks"_s, m_strategyConfig.choiceCooldownTicks},
    {u"stateActionCooldownTicks"_s, m_strategyConfig.stateActionCooldownTicks},
    {u"deprecatedLegacyStrategyParams"_s, true},
    {u"directionEmptyRuleTotal"_s, m_directionEmptyRuleTotal},
    {u"directionEmptyRuleWindow"_s, m_directionEmptyRuleWindow},
    {u"directionEmptyRuleWarnThreshold"_s, m_directionEmptyRuleWarnThreshold},
    {u"directionEmptyRuleWarnInterval"_s, m_directionEmptyRuleWarnInterval},
    {u"directionEmptyRuleWindowTicks"_s, m_directionEmptyRuleWindowTicks},
    {u"directionEmptySearchConsecutive"_s, m_directionEmptySearchConsecutive},
    {u"directionEmptySearchFuseThreshold"_s, m_directionEmptySearchFuseThreshold},
    {u"directionEmptySearchForceCenterTicks"_s, m_directionEmptySearchForceCenterTicks},
    {u"directionEmptySearchForceCenterDurationTicks"_s,
     m_directionEmptySearchForceCenterDurationTicks},
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
  ++m_runtimeTicks;
  if (m_directionEmptySearchForceCenterTicks > 0) {
    --m_directionEmptySearchForceCenterTicks;
  }
  if (m_directionEmptyRuleWindowTicks > 0 &&
      (m_runtimeTicks % m_directionEmptyRuleWindowTicks) == 0) {
    m_directionEmptyRuleWindow = 0;
  }
  pollMlOnlineModelHotReload();
}

auto State::observeDirectionEmptyRuleFallback(const bool usedFallback, const QString& reason)
  -> bool {
  if (!usedFallback || reason != QStringLiteral("direction-empty-rule")) {
    return false;
  }
  ++m_directionEmptyRuleTotal;
  ++m_directionEmptyRuleWindow;
  if (m_directionEmptyRuleWarnThreshold <= 0 ||
      m_directionEmptyRuleWindow < m_directionEmptyRuleWarnThreshold) {
    return false;
  }
  if (m_directionEmptyRuleWindow == m_directionEmptyRuleWarnThreshold) {
    return true;
  }
  return ((m_directionEmptyRuleWindow - m_directionEmptyRuleWarnThreshold) %
          std::max(1, m_directionEmptyRuleWarnInterval)) == 0;
}

void State::observeDirectionFallback(const bool usedFallback, const QString& reason) {
  if (usedFallback && reason == QStringLiteral("direction-empty-search")) {
    ++m_directionEmptySearchConsecutive;
    if (m_directionEmptySearchConsecutive >= m_directionEmptySearchFuseThreshold) {
      m_directionEmptySearchForceCenterTicks = std::max(
        m_directionEmptySearchForceCenterTicks, m_directionEmptySearchForceCenterDurationTicks);
      m_directionEmptySearchConsecutive = 0;
    }
    return;
  }
  if (!usedFallback || reason != QStringLiteral("direction-empty-search-circuit")) {
    m_directionEmptySearchConsecutive = 0;
  }
}

void State::resetDirectionEmptyRuleStats() {
  m_runtimeTicks = 0;
  m_directionEmptyRuleTotal = 0;
  m_directionEmptyRuleWindow = 0;
  m_directionEmptySearchConsecutive = 0;
  m_directionEmptySearchForceCenterTicks = 0;
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
