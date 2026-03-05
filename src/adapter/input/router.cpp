#include <QCoreApplication>

#include "adapter/engine_adapter.h"
#include "adapter/input/semantics.h"
#include "adapter/profile/bridge.h"
#include "adapter/ui/action.h"
#include "fsm/game_state.h"
#include "logging/categories.h"

using namespace Qt::StringLiterals;

void EngineAdapter::dispatchUiAction(const QString& action) {
  const nenoserpent::adapter::UiAction uiAction = nenoserpent::adapter::parseUiAction(action);
  dispatchUiAction(uiAction);
}

void EngineAdapter::dispatchUiAction(const nenoserpent::adapter::UiAction& action) {
  nenoserpent::adapter::dispatchUiAction(
    action,
    {
      .onMove = [this](const int dx, const int dy) -> void { move(dx, dy); },
      .onStart = [this]() -> void { handleStart(); },
      .onSecondary = [this]() -> void { handleBAction(); },
      .onSelect = [this]() -> void { handleSelect(); },
      .onBack = [this]() -> void {
        switch (nenoserpent::adapter::resolveBackActionForState(static_cast<int>(m_state))) {
        case nenoserpent::adapter::BackAction::QuitToMenu:
          quitToMenu();
          break;
        case nenoserpent::adapter::BackAction::QuitApplication:
          quit();
          break;
        case nenoserpent::adapter::BackAction::None:
          break;
        }
      },
      .onToggleShellColor = [this]() -> void { nextShellColor(); },
      .onToggleMusic = [this]() -> void { toggleMusic(); },
      .onToggleBot = [this]() -> void { cycleBotMode(); },
      .onToggleBotStrategy = [this]() -> void { cycleBotStrategyMode(); },
      .onQuitToMenu = [this]() -> void { quitToMenu(); },
      .onQuit = [this]() -> void { quit(); },
      .onNextPalette = [this]() -> void { nextPalette(); },
      .onDeleteSave = [this]() -> void { deleteSave(); },
      .onStateStartMenu = [this]() -> void { requestStateChange(AppState::StartMenu); },
      .onStateSplash = [this]() -> void { requestStateChange(AppState::Splash); },
      .onFeedbackLight = [this]() -> void { triggerHaptic(1); },
      .onFeedbackUi = [this]() -> void { triggerHaptic(5); },
      .onFeedbackHeavy = [this]() -> void { triggerHaptic(8); },
      .onSetLibraryIndex = [this](const int index) -> void { setLibraryIndex(index); },
      .onSetMedalIndex = [this](const int index) -> void { setMedalIndex(index); },
    });
}

void EngineAdapter::move(const int dx, const int dy) {
  dispatchStateCallback([dx, dy](GameState& state) -> void { state.handleInput(dx, dy); });

  if (m_state == AppState::Playing && m_sessionCore.enqueueDirection(QPoint(dx, dy))) {
    emit uiInteractTriggered();
  }
}

void EngineAdapter::nextPalette() {
  if (!m_profileManager) {
    return;
  }
  const int nextIdx = (nenoserpent::adapter::paletteIndex(m_profileManager.get()) + 1) % 5;
  nenoserpent::adapter::setPaletteIndex(m_profileManager.get(), nextIdx);
  emit paletteChanged();
  emit uiInteractTriggered();
}

void EngineAdapter::nextShellColor() {
  if (!m_profileManager) {
    return;
  }
  const int nextIdx = (nenoserpent::adapter::shellIndex(m_profileManager.get()) + 1) % 7;
  nenoserpent::adapter::setShellIndex(m_profileManager.get(), nextIdx);
  emit shellColorChanged();
  emit uiInteractTriggered();
}

void EngineAdapter::handleBAction() {
  // Unified semantics:
  // - B is always the secondary visual action (palette cycle).
  // - Back/menu navigation uses Select/Back action paths.
  if (m_state == AppState::StartMenu || m_state == AppState::Playing ||
      m_state == AppState::Paused || m_state == AppState::GameOver ||
      m_state == AppState::Replaying || m_state == AppState::ChoiceSelection ||
      m_state == AppState::Library || m_state == AppState::MedalRoom) {
    nextPalette();
  }
}

void EngineAdapter::quitToMenu() {
  if (m_state == AppState::Playing || m_state == AppState::Paused ||
      m_state == AppState::ChoiceSelection) {
    saveCurrentState();
  }
  requestStateChange(AppState::StartMenu);
}

void EngineAdapter::toggleMusic() {
  m_musicEnabled = !m_musicEnabled;
  qCInfo(nenoserpentAudioLog).noquote() << "toggleMusic ->" << m_musicEnabled;
  m_audioBus.handleMusicToggle(m_musicEnabled, static_cast<int>(m_state), m_bgmVariant);
  emit musicEnabledChanged();
}

void EngineAdapter::toggleBotAutoplay() {
  cycleBotMode();
}

void EngineAdapter::cycleBotMode() {
  if (m_botControlPort == nullptr) {
    return;
  }
  m_botControlPort->cycleBackendMode();
  emit botAutoplayChanged();
  emit botStrategyChanged();
  emit eventPrompt(u"BOT BACKEND: "_s + botModeName().toUpper());
  qCInfo(nenoserpentInputLog).noquote() << "bot backend ->" << botModeName();
}

void EngineAdapter::cycleBotStrategyMode() {
  if (m_botControlPort == nullptr) {
    return;
  }
  m_botControlPort->cycleStrategyMode();
  emit botStrategyChanged();
  const QString strategyMode = m_botControlPort->status().value(u"strategyMode"_s).toString();
  emit eventPrompt(u"BOT STRATEGY: "_s + strategyMode.toUpper());
  qCInfo(nenoserpentInputLog).noquote() << "bot strategy mode ->" << strategyMode;
}

void EngineAdapter::resetBotModeDefaults() {
  if (m_botControlPort == nullptr) {
    return;
  }
  m_botControlPort->resetStrategyModeDefaults();
  emit botStrategyChanged();
  emit eventPrompt(u"BOT RESET: MODE DEFAULT"_s);
  const QString strategyMode = m_botControlPort->status().value(u"strategyMode"_s).toString();
  qCInfo(nenoserpentInputLog).noquote() << "bot strategy reset for mode ->" << strategyMode;
}

auto EngineAdapter::setBotParam(const QString& key, int value) -> bool {
  if (m_botControlPort == nullptr || !m_botControlPort->setParam(key, value)) {
    return false;
  }

  const QString normalized = key.trimmed().toLower();
  emit botStrategyChanged();
  emit eventPrompt(u"BOT PARAM: "_s + key.toUpper() + u"="_s + QString::number(value));
  qCInfo(nenoserpentInputLog).noquote() << "bot param" << normalized << "->" << value;
  return true;
}

auto EngineAdapter::botStatus() const -> QVariantMap {
  return m_botControlPort != nullptr ? m_botControlPort->status() : QVariantMap{};
}

void EngineAdapter::cycleBgm() {
  const int variantCount = nenoserpent::audio::bgmVariantCount();
  m_bgmVariant = variantCount > 0 ? (m_bgmVariant + 1) % variantCount : 0;
  qCInfo(nenoserpentAudioLog).noquote() << "cycleBgm -> variant" << m_bgmVariant;

  emit eventPrompt(u"TRACK: "_s + nenoserpent::audio::bgmVariantName(m_bgmVariant).toString());

  if (!m_musicEnabled) {
    return;
  }

  if (m_state == AppState::StartMenu || m_state == AppState::Playing ||
      m_state == AppState::Replaying) {
    emit audioStartMusic(
      static_cast<int>(m_audioBus.musicTrackForState(static_cast<int>(m_state), m_bgmVariant)));
  }
}

void EngineAdapter::quit() {
  if (m_state == AppState::Playing || m_state == AppState::Paused ||
      m_state == AppState::ChoiceSelection) {
    saveCurrentState();
  }
  QCoreApplication::quit();
}

void EngineAdapter::handleSelect() {
  if (m_state == AppState::StartMenu) {
    nextLevel();
    return;
  }
  dispatchStateCallback([](GameState& state) -> void { state.handleSelect(); });
}

void EngineAdapter::handleStart() {
  dispatchStateCallback([](GameState& state) -> void { state.handleStart(); });
}

void EngineAdapter::deleteSave() {
  clearSavedState();
  // Clearing save should also reset level selection to default.
  m_levelIndex = 0;
  nenoserpent::adapter::setLevelIndex(m_profileManager.get(), m_levelIndex);
  loadLevelData(m_levelIndex);
  emit levelChanged();
}
