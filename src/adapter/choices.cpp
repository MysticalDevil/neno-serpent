#include <algorithm>
#include <cmath>

#include "adapter/engine.h"
#include "adapter/models/choice.h"
#include "adapter/models/library.h"
#include "adapter/profile/bridge.h"
#include "core/choice/runtime.h"
#include "fsm/game_state.h"
#include "power_up_id.h"

using namespace Qt::StringLiterals;

namespace {
constexpr int BuffDurationTicks = 40;

auto choiceSpecForType(const int type) -> std::optional<nenoserpent::core::ChoiceSpec> {
  using nenoserpent::core::ChoiceSpec;
  switch (type) {
  case PowerUpId::Ghost:
    return ChoiceSpec{
      .type = PowerUpId::Ghost, .name = u"Ghost"_s, .description = u"Pass through self"_s};
  case PowerUpId::Slow:
    return ChoiceSpec{
      .type = PowerUpId::Slow, .name = u"Slow"_s, .description = u"Drop speed by 1 tier"_s};
  case PowerUpId::Magnet:
    return ChoiceSpec{
      .type = PowerUpId::Magnet, .name = u"Magnet"_s, .description = u"Attract food"_s};
  case PowerUpId::Shield:
    return ChoiceSpec{
      .type = PowerUpId::Shield, .name = u"Shield"_s, .description = u"One extra life"_s};
  case PowerUpId::Portal:
    return ChoiceSpec{
      .type = PowerUpId::Portal, .name = u"Portal"_s, .description = u"Phase through walls"_s};
  case PowerUpId::Double:
    return ChoiceSpec{
      .type = PowerUpId::Double, .name = u"Double"_s, .description = u"Double points"_s};
  case PowerUpId::Rich:
    return ChoiceSpec{
      .type = PowerUpId::Rich, .name = u"Diamond"_s, .description = u"Triple points"_s};
  case PowerUpId::Laser:
    return ChoiceSpec{
      .type = PowerUpId::Laser, .name = u"Laser"_s, .description = u"Break obstacle"_s};
  case PowerUpId::Mini:
    return ChoiceSpec{.type = PowerUpId::Mini, .name = u"Mini"_s, .description = u"Shrink body"_s};
  default:
    return std::nullopt;
  }
}

auto buildDebugChoiceSpecs(const QVariantList& types) -> QList<nenoserpent::core::ChoiceSpec> {
  QList<nenoserpent::core::ChoiceSpec> result;
  QList<int> seenTypes;

  for (const QVariant& entry : types) {
    bool ok = false;
    const int type = entry.toInt(&ok);
    if (!ok || seenTypes.contains(type)) {
      continue;
    }
    const auto spec = choiceSpecForType(type);
    if (!spec.has_value()) {
      continue;
    }
    result.append(spec.value());
    seenTypes.append(type);
    if (result.size() >= 3) {
      return result;
    }
  }

  for (int type = PowerUpId::Ghost; type <= PowerUpId::Mini && result.size() < 3; ++type) {
    if (seenTypes.contains(type)) {
      continue;
    }
    const auto spec = choiceSpecForType(type);
    if (!spec.has_value()) {
      continue;
    }
    result.append(spec.value());
  }

  return result;
}

auto choiceRecoveryWindowMs(const int preChoiceTickIntervalMs, const bool slowMode) -> int {
  const int clampedInterval = std::clamp(preChoiceTickIntervalMs, 60, 200);
  const int fastness = 200 - clampedInterval;    // 0 (slow) .. 140 (fast)
  int windowMs = 900 + ((fastness * 900) / 140); // 900..1800
  if (slowMode) {
    windowMs = std::max(700, windowMs - 220);
  }
  return std::clamp(windowMs, 700, 1800);
}

auto choiceRecoveryStartIntervalMs(const int preChoiceTickIntervalMs, const bool slowMode) -> int {
  const int clamped = std::clamp(preChoiceTickIntervalMs, 60, 200);
  const int extra = slowMode ? 110 : 170;
  return std::clamp(clamped + extra, clamped + 60, 320);
}

auto easeInOutCubic(const float t) -> float {
  const float clamped = std::clamp(t, 0.0F, 1.0F);
  if (clamped < 0.5F) {
    return 4.0F * clamped * clamped * clamped;
  }
  const float remain = (-2.0F * clamped) + 2.0F;
  return 1.0F - ((remain * remain * remain) / 2.0F);
}
} // namespace

void EngineAdapter::generateChoices() {
  const QList<nenoserpent::core::ChoiceSpec> allChoices =
    nenoserpent::core::pickRoguelikeChoices(m_rng.generate(), 3);
  m_choices = nenoserpent::adapter::buildChoiceModel(allChoices);
  emit choicesChanged();
}

void EngineAdapter::debugSeedChoicePreview(const QVariantList& types) {
  stopEngineTimer();
  resetTransientRuntimeState();
  loadLevelData(m_levelIndex);
  m_sessionCore.applyMetaAction(nenoserpent::core::MetaAction::seedPreviewState({
    .obstacles = m_session.obstacles,
    .body = {{10, 4}, {10, 5}, {10, 6}, {10, 7}},
    .food = QPoint(12, 7),
    .direction = QPoint(0, -1),
    .powerUpPos = QPoint(-1, -1),
    .powerUpType = 0,
    .score = 42,
    .tickCounter = 64,
  }));
  syncSnakeModelFromCore();
  m_choices = nenoserpent::adapter::buildChoiceModel(buildDebugChoiceSpecs(types));
  m_choiceIndex = 0;

  emit scoreChanged();
  emit foodChanged();
  emit powerUpChanged();
  emit buffChanged();
  emit ghostChanged();
  emit choicesChanged();
  emit choiceIndexChanged();

  setInternalState(AppState::ChoiceSelection);
}

void EngineAdapter::selectChoice(const int index) {
  if (index < 0 || index >= m_choices.size()) {
    return;
  }

  if (m_state != AppState::Replaying) {
    m_currentChoiceHistory.append({.frame = m_sessionCore.tickCounter(), .index = index});
  }

  const auto type = nenoserpent::adapter::choiceTypeAt(m_choices, index);
  if (!type.has_value()) {
    return;
  }
  const int preChoiceTickIntervalMs = gameplayTickIntervalMs();
  const auto result = m_sessionCore.selectChoice(type.value(), BuffDurationTicks * 2, false);
  if (m_state != AppState::Replaying &&
      nenoserpent::adapter::discoverFruit(m_profileManager.get(), type.value())) {
    emit fruitLibraryChanged();
    emit eventPrompt(u"CATALOG UNLOCK: "_s + nenoserpent::adapter::fruitNameForType(type.value()));
  }
  if (m_state != AppState::Replaying && type.value() == PowerUpId::Ghost) {
    nenoserpent::adapter::logGhostTrigger(m_profileManager.get());
  }
  if (result.miniApplied) {
    syncSnakeModelFromCore();
    emit eventPrompt(u"MINI BLITZ! SIZE CUT"_s);
  }

  emit buffChanged();
  if (m_state == AppState::Replaying) {
    cancelChoiceSpeedRecovery();
    m_timer->setInterval(gameplayTickIntervalMs());
    return;
  }

  startChoiceSpeedRecovery(preChoiceTickIntervalMs, result.slowMode);
  requestStateChange(AppState::Playing);
}

void EngineAdapter::startChoiceSpeedRecovery(const int preChoiceTickIntervalMs,
                                             const bool slowMode) {
  const int targetInterval = gameplayTickIntervalMs();
  const int startInterval =
    std::max(choiceRecoveryStartIntervalMs(preChoiceTickIntervalMs, slowMode), targetInterval + 30);
  m_choiceSpeedRecoveryActive = (startInterval > targetInterval + 2);
  m_choiceSpeedRecoveryElapsedMs = 0;
  m_choiceSpeedRecoveryDurationMs = choiceRecoveryWindowMs(preChoiceTickIntervalMs, slowMode);
  m_choiceSpeedRecoveryStartIntervalMs = startInterval;
  m_choiceSpeedRecoveryTargetIntervalMs = targetInterval;

  m_timer->setInterval(m_choiceSpeedRecoveryActive ? startInterval : targetInterval);
}

void EngineAdapter::advanceChoiceSpeedRecovery() {
  if (!m_choiceSpeedRecoveryActive || m_state != AppState::Playing) {
    return;
  }

  m_choiceSpeedRecoveryTargetIntervalMs = gameplayTickIntervalMs();
  if (m_choiceSpeedRecoveryStartIntervalMs <= m_choiceSpeedRecoveryTargetIntervalMs) {
    cancelChoiceSpeedRecovery();
    m_timer->setInterval(m_choiceSpeedRecoveryTargetIntervalMs);
    return;
  }

  const int stepMs = std::max(1, m_timer->interval());
  m_choiceSpeedRecoveryElapsedMs += stepMs;
  const int durationMs = std::max(1, m_choiceSpeedRecoveryDurationMs);
  const float progress =
    static_cast<float>(m_choiceSpeedRecoveryElapsedMs) / static_cast<float>(durationMs);
  const float eased = easeInOutCubic(progress);
  const int span = m_choiceSpeedRecoveryStartIntervalMs - m_choiceSpeedRecoveryTargetIntervalMs;
  const int nextInterval =
    m_choiceSpeedRecoveryStartIntervalMs - static_cast<int>(std::lround(span * eased));

  const int currentInterval = m_timer->interval();
  const int boundedNext = std::clamp(
    nextInterval, m_choiceSpeedRecoveryTargetIntervalMs, m_choiceSpeedRecoveryStartIntervalMs);
  const int maxDeltaPerTick = 3;
  const int smoothed = std::clamp(
    boundedNext, currentInterval - maxDeltaPerTick, currentInterval + maxDeltaPerTick);
  m_timer->setInterval(smoothed);
  if (progress >= 1.0F || m_timer->interval() <= m_choiceSpeedRecoveryTargetIntervalMs) {
    cancelChoiceSpeedRecovery();
    m_timer->setInterval(gameplayTickIntervalMs());
  }
}

void EngineAdapter::cancelChoiceSpeedRecovery() {
  m_choiceSpeedRecoveryActive = false;
  m_choiceSpeedRecoveryElapsedMs = 0;
  m_choiceSpeedRecoveryDurationMs = 0;
  m_choiceSpeedRecoveryStartIntervalMs = 0;
  m_choiceSpeedRecoveryTargetIntervalMs = 0;
}
