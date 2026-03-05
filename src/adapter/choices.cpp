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
      .type = PowerUpId::Slow, .name = u"Slow"_s, .description = u"Decrease speed"_s};
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
    m_timer->setInterval(gameplayTickIntervalMs());
    return;
  }

  m_timer->setInterval(500);

  QTimer::singleShot(500, this, [this]() -> void {
    if (m_state == AppState::Playing) {
      m_timer->setInterval(gameplayTickIntervalMs());
    }
  });

  requestStateChange(AppState::Playing);
}
