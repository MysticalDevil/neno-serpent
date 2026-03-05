#include "adapter/ui/controller.h"

#include "adapter/bot/port.h"
#include "adapter/engine_adapter.h"

using namespace Qt::StringLiterals;

UiCommandController::UiCommandController(EngineAdapter* engineAdapter, QObject* parent)
    : QObject(parent),
      m_engineAdapter(engineAdapter) {
  if (m_engineAdapter != nullptr) {
    m_botControlPort = m_engineAdapter->botControlPort();
  }
  if (m_engineAdapter == nullptr) {
    return;
  }

  connect(
    m_engineAdapter, &EngineAdapter::paletteChanged, this, &UiCommandController::paletteChanged);
  connect(
    m_engineAdapter, &EngineAdapter::shellColorChanged, this, &UiCommandController::shellChanged);
  connect(m_engineAdapter,
          &EngineAdapter::achievementEarned,
          this,
          &UiCommandController::achievementEarned);
  connect(m_engineAdapter, &EngineAdapter::eventPrompt, this, &UiCommandController::eventPrompt);
}

void UiCommandController::dispatch(const QString& action) const {
  if (m_engineAdapter != nullptr) {
    m_engineAdapter->dispatchUiAction(action);
  }
}

void UiCommandController::requestStateChange(const int state) const {
  if (m_engineAdapter != nullptr) {
    m_engineAdapter->requestStateChange(state);
  }
}

void UiCommandController::cycleBgm() const {
  if (m_engineAdapter != nullptr) {
    m_engineAdapter->cycleBgm();
  }
}

void UiCommandController::cycleBotMode() {
  if (m_botControlPort != nullptr) {
    m_botControlPort->cycleBackendMode();
    emit eventPrompt(u"BOT BACKEND: "_s + m_botControlPort->modeName().toUpper());
  }
}

void UiCommandController::cycleBotStrategyMode() {
  if (m_botControlPort != nullptr) {
    m_botControlPort->cycleStrategyMode();
    const QString mode = m_botControlPort->status().value(u"strategyMode"_s).toString();
    emit eventPrompt(u"BOT STRATEGY: "_s + mode.toUpper());
  }
}

void UiCommandController::resetBotModeDefaults() {
  if (m_botControlPort != nullptr) {
    m_botControlPort->resetStrategyModeDefaults();
    emit eventPrompt(u"BOT RESET: MODE DEFAULT"_s);
  }
}

auto UiCommandController::setBotParam(const QString& key, const int value) -> bool {
  if (m_botControlPort != nullptr) {
    if (!m_botControlPort->setParam(key, value)) {
      return false;
    }
    emit eventPrompt(u"BOT PARAM: "_s + key.toUpper() + u"="_s + QString::number(value));
    return true;
  }
  return false;
}

auto UiCommandController::botStatus() const -> QVariantMap {
  if (m_botControlPort != nullptr) {
    return m_botControlPort->status();
  }
  return {};
}

void UiCommandController::seedChoicePreview(const QVariantList& types) const {
  if (m_engineAdapter != nullptr) {
    m_engineAdapter->debugSeedChoicePreview(types);
  }
}

void UiCommandController::seedReplayBuffPreview() const {
  if (m_engineAdapter != nullptr) {
    m_engineAdapter->debugSeedReplayBuffPreview();
  }
}
