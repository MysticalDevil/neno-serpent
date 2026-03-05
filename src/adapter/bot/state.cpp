#include "adapter/bot/state.h"

#include <algorithm>
#include <optional>
#include <utility>

#include <QVariantMap>

#include "logging/categories.h"

using namespace Qt::StringLiterals;

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
  if (value == QStringLiteral("rule")) {
    return BotBackendMode::Rule;
  }
  if (value == QStringLiteral("ml")) {
    return BotBackendMode::Ml;
  }
  if (value == QStringLiteral("search")) {
    return BotBackendMode::Search;
  }
  return std::nullopt;
}

} // namespace

void State::initializeFromEnvironment() {
  const QString botStrategyOverride = qEnvironmentVariable("NENOSERPENT_BOT_STRATEGY_FILE");
  const auto strategyLoad = loadStrategyConfig(currentBuildProfileName(), botStrategyOverride);
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

  const QString minConfidenceRaw = qEnvironmentVariable("NENOSERPENT_BOT_ML_MIN_CONF").trimmed();
  const QString minMarginRaw = qEnvironmentVariable("NENOSERPENT_BOT_ML_MIN_MARGIN").trimmed();
  const auto [minConfidence, confOk] = parseFloatOrDefault(minConfidenceRaw, 0.90F);
  const auto [minMargin, marginOk] = parseFloatOrDefault(minMarginRaw, 1.20F);
  m_mlBackend.setConfidenceGate(minConfidence, minMargin);
  qCInfo(nenoserpentInputLog).noquote()
    << "bot ml gate conf=" << minConfidence << "margin=" << minMargin;
  if ((!minConfidenceRaw.isEmpty() && !confOk) || (!minMarginRaw.isEmpty() && !marginOk)) {
    qCWarning(nenoserpentInputLog).noquote()
      << "invalid ml gate env override, using defaults when parse fails";
  }

  const QString mlModelPath = qEnvironmentVariable("NENOSERPENT_BOT_ML_MODEL").trimmed();
  if (!mlModelPath.isEmpty()) {
    if (m_mlBackend.loadFromFile(mlModelPath)) {
      qCInfo(nenoserpentInputLog).noquote() << "bot ml model loaded source=" << mlModelPath;
    } else {
      qCWarning(nenoserpentInputLog).noquote() << "bot ml model unavailable source=" << mlModelPath
                                               << "reason=" << m_mlBackend.errorString();
    }
  } else {
    qCInfo(nenoserpentInputLog).noquote() << "bot ml model not configured";
  }

  const QString backendOverride =
    qEnvironmentVariable("NENOSERPENT_BOT_BACKEND").trimmed().toLower();
  if (backendOverride.isEmpty()) {
    return;
  }
  if (const auto mode = parseBackendModeOverride(backendOverride); mode.has_value()) {
    m_backendMode = *mode;
    qCInfo(nenoserpentInputLog).noquote() << "bot backend override ->" << backendOverride;
    return;
  }
  qCWarning(nenoserpentInputLog).noquote() << "invalid bot backend override:" << backendOverride
                                           << "(expected off|rule|ml|search, fallback off)";
  m_backendMode = BotBackendMode::Off;
}

auto State::autoplayEnabled() const -> bool {
  return m_backendMode != BotBackendMode::Off;
}

auto State::backendModeName() const -> QString {
  return nenoserpent::adapter::bot::backendModeName(m_backendMode);
}

auto State::strategyModeName() const -> QString {
  return nenoserpent::adapter::bot::modeName(m_strategyMode);
}

void State::cycleBackendMode() {
  m_backendMode = nextBackendMode(m_backendMode);
  resetActionCooldownTicks();
  clearLastBackendRoute();
}

void State::cycleStrategyMode() {
  m_strategyMode = nextMode(m_strategyMode);
  resetActionCooldownTicks();
  applyModeDefaults();
}

void State::resetStrategyModeDefaults() {
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
    {u"strategyMode"_s, strategyModeName()},
    {u"openSpaceWeight"_s, m_strategyConfig.openSpaceWeight},
    {u"safeNeighborWeight"_s, m_strategyConfig.safeNeighborWeight},
    {u"targetDistanceWeight"_s, m_strategyConfig.targetDistanceWeight},
    {u"straightBonus"_s, m_strategyConfig.straightBonus},
    {u"foodConsumeBonus"_s, m_strategyConfig.foodConsumeBonus},
    {u"trapPenalty"_s, m_strategyConfig.trapPenalty},
    {u"lookaheadDepth"_s, m_strategyConfig.lookaheadDepth},
    {u"lookaheadWeight"_s, m_strategyConfig.lookaheadWeight},
    {u"choiceCooldownTicks"_s, m_strategyConfig.choiceCooldownTicks},
    {u"stateActionCooldownTicks"_s, m_strategyConfig.stateActionCooldownTicks},
  };
}

auto State::currentBackend() const -> const BotBackend* {
  switch (m_backendMode) {
  case BotBackendMode::Off:
    return nullptr;
  case BotBackendMode::Rule:
    return &ruleBackend();
  case BotBackendMode::Ml:
    return &m_mlBackend;
  case BotBackendMode::Search:
    return &searchBackend();
  }
  return nullptr;
}

void State::applyModeDefaults() {
  m_strategyConfig = m_baseStrategyConfig;
  nenoserpent::adapter::bot::applyModeDefaults(m_strategyConfig, m_strategyMode);
}

} // namespace nenoserpent::adapter::bot
