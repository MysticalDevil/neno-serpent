#include "adapter/engine_adapter.h"

#include <QDateTime>
#include <QDebug>

#include "fsm/game_state.h"
#include "fsm/state_factory.h"
#include "logging/categories.h"
#include "profile_manager.h"
#ifdef NENOSERPENT_HAS_SENSORS
#include <QAccelerometer>
#endif

#include <algorithm>
#include <utility>

namespace {
auto stateName(const int state) -> const char* {
  switch (state) {
  case AppState::Splash:
    return "Splash";
  case AppState::StartMenu:
    return "StartMenu";
  case AppState::Playing:
    return "Playing";
  case AppState::Paused:
    return "Paused";
  case AppState::GameOver:
    return "GameOver";
  case AppState::Replaying:
    return "Replaying";
  case AppState::ChoiceSelection:
    return "ChoiceSelection";
  case AppState::Library:
    return "Library";
  case AppState::MedalRoom:
    return "MedalRoom";
  default:
    return "Unknown";
  }
}

} // namespace

EngineAdapter::EngineAdapter(QObject* parent)
    : QObject(parent),
      m_rng(QRandomGenerator::securelySeeded()),
      m_session(m_sessionCore.state()),
      m_timer(std::make_unique<QTimer>()),
#ifdef NENOSERPENT_HAS_SENSORS
      m_accelerometer(std::make_unique<QAccelerometer>()),
#endif
      m_profileManager(std::make_unique<ProfileManager>()),
      m_inputQueue(m_sessionCore.inputQueue()),
      m_fsmState(nullptr) {
  m_timer->setTimerType(Qt::PreciseTimer);
  const QString botStrategyOverride = qEnvironmentVariable("NENOSERPENT_BOT_STRATEGY_FILE");
  const auto strategyLoad = nenoserpent::adapter::bot::loadStrategyConfig(
    nenoserpent::adapter::bot::currentBuildProfileName(), botStrategyOverride);
  m_botBaseStrategyConfig = strategyLoad.config;
  m_botStrategyConfig = m_botBaseStrategyConfig;
  applyBotModeDefaults();
  if (strategyLoad.loaded) {
    qCInfo(nenoserpentInputLog).noquote()
      << "bot strategy loaded profile=" << strategyLoad.profile << "source=" << strategyLoad.source;
  } else {
    qCWarning(nenoserpentInputLog).noquote()
      << "bot strategy fallback to defaults profile=" << strategyLoad.profile
      << "source=" << strategyLoad.source << "reason=" << strategyLoad.error;
  }

  const QString mlModelPath = qEnvironmentVariable("NENOSERPENT_BOT_ML_MODEL").trimmed();
  const QString minConfidenceRaw = qEnvironmentVariable("NENOSERPENT_BOT_ML_MIN_CONF").trimmed();
  const QString minMarginRaw = qEnvironmentVariable("NENOSERPENT_BOT_ML_MIN_MARGIN").trimmed();
  const auto parseFloatOrDefault = [](const QString& text,
                                      const float fallback) -> std::pair<float, bool> {
    if (text.isEmpty()) {
      return {fallback, true};
    }
    bool ok = false;
    const float value = text.toFloat(&ok);
    return {ok ? value : fallback, ok};
  };
  const auto [minConfidence, confOk] = parseFloatOrDefault(minConfidenceRaw, 0.90F);
  const auto [minMargin, marginOk] = parseFloatOrDefault(minMarginRaw, 1.20F);
  m_botMlBackend.setConfidenceGate(minConfidence, minMargin);
  qCInfo(nenoserpentInputLog).noquote()
    << "bot ml gate conf=" << minConfidence << "margin=" << minMargin;
  if ((!minConfidenceRaw.isEmpty() && !confOk) || (!minMarginRaw.isEmpty() && !marginOk)) {
    qCWarning(nenoserpentInputLog).noquote()
      << "invalid ml gate env override, using defaults when parse fails";
  }
  if (!mlModelPath.isEmpty()) {
    if (m_botMlBackend.loadFromFile(mlModelPath)) {
      qCInfo(nenoserpentInputLog).noquote() << "bot ml model loaded source=" << mlModelPath;
    } else {
      qCWarning(nenoserpentInputLog).noquote() << "bot ml model unavailable source=" << mlModelPath
                                               << "reason=" << m_botMlBackend.errorString();
    }
  } else {
    qCInfo(nenoserpentInputLog).noquote() << "bot ml model not configured";
  }

  const QString backendOverride =
    qEnvironmentVariable("NENOSERPENT_BOT_BACKEND").trimmed().toLower();
  if (!backendOverride.isEmpty()) {
    if (backendOverride == QStringLiteral("off")) {
      m_botBackendMode = nenoserpent::adapter::bot::BotBackendMode::Off;
      qCInfo(nenoserpentInputLog).noquote() << "bot backend override -> off";
    } else if (backendOverride == QStringLiteral("rule")) {
      m_botBackendMode = nenoserpent::adapter::bot::BotBackendMode::Rule;
      qCInfo(nenoserpentInputLog).noquote() << "bot backend override -> rule";
    } else if (backendOverride == QStringLiteral("ml")) {
      m_botBackendMode = nenoserpent::adapter::bot::BotBackendMode::Ml;
      qCInfo(nenoserpentInputLog).noquote() << "bot backend override -> ml";
    } else {
      qCWarning(nenoserpentInputLog).noquote() << "invalid bot backend override:" << backendOverride
                                               << "(expected off|rule|ml, fallback off)";
      m_botBackendMode = nenoserpent::adapter::bot::BotBackendMode::Off;
    }
  }

  m_audioBus.setCallbacks({
    .startMusic = [this](const nenoserpent::audio::ScoreTrackId trackId) -> void {
      emit audioStartMusic(static_cast<int>(trackId));
    },
    .stopMusic = [this]() -> void { emit audioStopMusic(); },
    .setPaused = [this](const bool paused) -> void { emit audioSetPaused(paused); },
    .setMusicEnabled = [this](const bool enabled) -> void { emit audioSetMusicEnabled(enabled); },
    .duckMusic = [this](const float scale, const int durationMs) -> void {
      emit audioDuckMusic(scale, durationMs);
    },
    .setVolume = [this](const float volume) -> void { emit audioSetVolume(volume); },
    .setScore = [this](const int score) -> void { emit audioSetScore(score); },
    .playBeep = [this](const int frequencyHz, const int durationMs, const float pan) -> void {
      emit audioPlayBeep(frequencyHz, durationMs, pan);
    },
    .playScoreCue = [this](const nenoserpent::audio::ScoreCueId cueId, const float pan) -> void {
      emit audioPlayScoreCue(static_cast<int>(cueId), pan);
    },
    .playCrash = [this](const int durationMs) -> void { emit audioPlayCrash(durationMs); },
  });

  connect(m_timer.get(), &QTimer::timeout, this, &EngineAdapter::update);
  setupAudioSignals();
  setupSensorRuntime();
  m_snakeModel.setBodyChangedCallback(
    [this](const std::deque<QPoint>& body) { m_sessionCore.setBody(body); });

  m_sessionCore.setBody({{10, 10}, {10, 11}, {10, 12}});
  syncSnakeModelFromCore();
}

EngineAdapter::~EngineAdapter() {
#ifdef NENOSERPENT_HAS_SENSORS
  if (m_accelerometer) {
    m_accelerometer->stop();
  }
#endif
  if (m_timer) {
    m_timer->stop();
  }
  m_fsmState.reset();
}

void EngineAdapter::syncSnakeModelFromCore() {
  m_snakeModel.reset(m_sessionCore.body());
}

void EngineAdapter::setupAudioSignals() {
  connect(this, &EngineAdapter::foodEaten, this, [this](const float pan) -> void {
    m_audioBus.dispatchEvent(nenoserpent::audio::Event::Food,
                             {.score = m_session.score, .pan = pan});
    triggerHaptic(3);
  });

  connect(this, &EngineAdapter::powerUpEaten, this, [this]() -> void {
    m_audioBus.dispatchEvent(nenoserpent::audio::Event::PowerUp);
    triggerHaptic(6);
  });

  connect(this, &EngineAdapter::playerCrashed, this, [this]() -> void {
    m_audioBus.dispatchEvent(nenoserpent::audio::Event::Crash);
    triggerHaptic(12);
  });

  connect(this, &EngineAdapter::uiInteractTriggered, this, [this]() -> void {
#ifdef Q_OS_ANDROID
    constexpr qint64 MinAudioEventIntervalMs = 80;
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    if (nowMs - m_lastUiInteractAudioMs < MinAudioEventIntervalMs) {
      return;
    }
    m_lastUiInteractAudioMs = nowMs;
#endif
    m_audioBus.dispatchEvent(nenoserpent::audio::Event::UiInteract);
    triggerHaptic(2);
  });

  connect(this, &EngineAdapter::stateChanged, this, [this]() -> void {
    qCInfo(nenoserpentAudioLog).noquote()
      << "stateChanged ->" << stateName(m_state) << "(musicEnabled=" << m_musicEnabled << ")";
    const int token = m_audioStateToken;
    m_audioBus.handleStateChanged(
      static_cast<int>(m_state),
      m_musicEnabled,
      m_bgmVariant,
      [this, token](const int delayMs, const std::function<void()>& callback) -> void {
        QTimer::singleShot(delayMs, this, [this, token, callback]() -> void {
          if (token != m_audioStateToken) {
            qCDebug(nenoserpentAudioLog).noquote() << "menu BGM deferred start canceled by token";
            return;
          }
          if (callback) {
            callback();
          }
        });
      });
  });
}

void EngineAdapter::setupSensorRuntime() {
#ifdef NENOSERPENT_HAS_SENSORS
  if (!m_accelerometer) {
    return;
  }

  m_accelerometer->setDataRate(30);
  connect(m_accelerometer.get(), &QAccelerometer::readingChanged, this, [this]() -> void {
    if (!m_accelerometer || !m_accelerometer->reading()) {
      return;
    }
    constexpr qreal maxTilt = 6.0;
    const qreal nx = std::clamp(m_accelerometer->reading()->y() / maxTilt, -1.0, 1.0);
    const qreal ny = std::clamp(m_accelerometer->reading()->x() / maxTilt, -1.0, 1.0);
    m_reflectionOffset = QPointF(nx * 0.02, -ny * 0.02);
    m_hasAccelerometerReading = true;
    emit reflectionOffsetChanged();
  });
  m_accelerometer->start();
  QTimer::singleShot(200, this, [this]() -> void {
    if (!m_accelerometer) {
      return;
    }
    qCInfo(nenoserpentStateLog).noquote()
      << "accelerometer connected=" << m_accelerometer->isConnectedToBackend()
      << "active=" << m_accelerometer->isActive();
  });
#else
  m_hasAccelerometerReading = false;
#endif
}

void EngineAdapter::setInternalState(const int s) {
  const auto next = static_cast<AppState::Value>(s);
  if (m_state != next) {
    qCInfo(nenoserpentStateLog).noquote()
      << "setInternalState:" << stateName(m_state) << "->" << stateName(next);
    m_state = next;
    m_audioStateToken++;
    m_audioBus.syncPausedState(static_cast<int>(m_state));
    emit stateChanged();
  }
}

void EngineAdapter::requestStateChange(const int newState) {
  if (m_stateCallbackInProgress) {
    qCDebug(nenoserpentStateLog).noquote()
      << "defer requestStateChange to" << stateName(newState) << "(inside callback)";
    m_pendingStateChange = newState;
    return;
  }
  qCInfo(nenoserpentStateLog).noquote() << "requestStateChange ->" << stateName(newState);

  if (auto nextState = nenoserpent::fsm::createStateFor(*this, newState); nextState) {
    changeState(std::move(nextState));
  }
}

void EngineAdapter::startEngineTimer(const int intervalMs) {
  if (intervalMs > 0) {
    m_timer->setInterval(std::max(30, intervalMs));
  } else {
    m_timer->setInterval(gameplayTickIntervalMs());
  }
  m_timer->start();
}

auto EngineAdapter::initialGameplayIntervalMs() const -> int {
#ifdef Q_OS_ANDROID
  return 140;
#else
  return 200;
#endif
}

auto EngineAdapter::gameplayTickIntervalMs() const -> int {
  const int interval = m_sessionCore.currentTickIntervalMs();
#ifdef Q_OS_ANDROID
  return std::max(50, interval);
#else
  return interval;
#endif
}

void EngineAdapter::stopEngineTimer() {
  m_timer->stop();
}

void EngineAdapter::triggerHaptic(const int magnitude) {
  emit requestFeedback(magnitude);
  m_haptics.trigger(magnitude);
}

void EngineAdapter::emitAudioEvent(const nenoserpent::audio::Event event, const float pan) {
  qCDebug(nenoserpentAudioLog).noquote()
    << "emitAudioEvent =" << static_cast<int>(event) << " pan=" << pan;
  switch (event) {
  case nenoserpent::audio::Event::Food:
    emit foodEaten(pan);
    break;
  case nenoserpent::audio::Event::Crash:
    emit playerCrashed();
    break;
  case nenoserpent::audio::Event::UiInteract:
    emit uiInteractTriggered();
    break;
  case nenoserpent::audio::Event::Confirm:
    m_audioBus.dispatchEvent(nenoserpent::audio::Event::Confirm);
    break;
  case nenoserpent::audio::Event::PowerUp:
    emit powerUpEaten();
    break;
  }
}

void EngineAdapter::applyPendingStateChangeIfNeeded() {
  if (!m_pendingStateChange.has_value()) {
    return;
  }
  const int pendingState = *m_pendingStateChange;
  m_pendingStateChange.reset();
  requestStateChange(pendingState);
}

void EngineAdapter::dispatchStateCallback(const std::function<void(GameState&)>& callback) {
  if (!m_fsmState) {
    return;
  }
  // Defer state replacement while executing a state callback to avoid invalidating the state
  // object mid-function.
  m_stateCallbackInProgress = true;
  callback(*m_fsmState);
  m_stateCallbackInProgress = false;
  applyPendingStateChangeIfNeeded();
}

void EngineAdapter::changeState(std::unique_ptr<GameState> newState) {
  if (m_fsmState) {
    m_fsmState->exit();
  }
  m_fsmState = std::move(newState);
  if (m_fsmState) {
    m_fsmState->enter();
  }
}
