#include <cstdio>
#include <cstdlib>

#include <QDebug>
#include <QGuiApplication>
#include <QIcon>
#include <QLocale>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QTimer>
#include <QTranslator>
#include <qqml.h>

#include "adapter/engine.h"
#include "adapter/ui/controller.h"
#include "adapter/view_models/audio.h"
#include "adapter/view_models/render.h"
#include "adapter/view_models/selection.h"
#include "adapter/view_models/status.h"
#include "adapter/view_models/theme.h"
#include "app_state.h"
#include "input_injection_pipe.h"
#include "logging/categories.h"
#include "logging/mode.h"
#include "logging/policy.h"
#include "logging/runtime_logger.h"
#include "power_up_id.h"
#include "sound_manager.h"

namespace {
#if defined(QT_NO_DEBUG_OUTPUT) && defined(QT_NO_INFO_OUTPUT) && defined(QT_NO_WARNING_OUTPUT)
auto releaseLogFilter(QtMsgType type, const QMessageLogContext& logContext, const QString& msg)
  -> void {
  Q_UNUSED(logContext);
  Q_UNUSED(msg);
  if (type == QtFatalMsg) {
    abort();
  }
}

void silenceStderrForRelease() {
#ifdef _WIN32
  if (freopen("NUL", "w", stderr) == nullptr) { // NOLINT(cppcoreguidelines-owning-memory)
    return;
  }
#else
  if (freopen("/dev/null", "w", stderr) == nullptr) { // NOLINT(cppcoreguidelines-owning-memory)
    return;
  }
#endif
}
#endif
} // namespace

auto main(int argc, char* argv[]) -> int {
#if defined(QT_NO_DEBUG_OUTPUT) && defined(QT_NO_INFO_OUTPUT) && defined(QT_NO_WARNING_OUTPUT)
  const bool keepStderr = qEnvironmentVariableIntValue("NENOSERPENT_KEEP_STDERR") == 1;
  if (!keepStderr) {
    silenceStderrForRelease();
    qInstallMessageHandler(releaseLogFilter);
  }
#endif

  QGuiApplication app(argc, argv);
  const nenoserpent::logging::LogMode logMode = nenoserpent::logging::detectBuildLogMode();
  nenoserpent::logging::applyLoggingPolicy(logMode);
  const QString appLogMode = QString::fromLatin1(nenoserpent::logging::logModeName(logMode));
  QString uiMode = "full";
  const QStringList arguments = QCoreApplication::arguments();
  for (const QString& argument : arguments) {
    if (argument.startsWith("--ui-mode=")) {
      const QString candidate = argument.sliced(QString("--ui-mode=").size()).trimmed().toLower();
      if (candidate == "full" || candidate == "screen" || candidate == "shell") {
        uiMode = candidate;
      }
    }
  }

  QCoreApplication::setOrganizationName("devil");
  QCoreApplication::setOrganizationDomain("org.devil.nenoserpent");
  QCoreApplication::setApplicationName("nenoserpent");
  QGuiApplication::setApplicationDisplayName("NenoSerpent");
  QGuiApplication::setApplicationVersion("2.0.0");

  qCInfo(nenoserpentStateLog).noquote() << "build mode" << appLogMode << "logging enabled";

  EngineAdapter engineAdapter;
  UiCommandController uiCommandController(&engineAdapter);
  AudioSettingsViewModel audioSettingsViewModel(&engineAdapter);
  SelectionViewModel selectionViewModel(&engineAdapter);
  SessionRenderViewModel sessionRenderViewModel(&engineAdapter);
  SessionStatusViewModel sessionStatusViewModel(&engineAdapter);
  ThemeViewModel themeViewModel(&engineAdapter);
  SoundManager soundManager;
  InputInjectionPipe inputInjectionPipe;
  RuntimeLogger runtimeLogger;
  soundManager.setVolume(engineAdapter.volume());

  QObject::connect(
    &engineAdapter, &EngineAdapter::audioPlayBeep, &soundManager, &SoundManager::playBeep);
  QObject::connect(
    &engineAdapter, &EngineAdapter::audioPlayScoreCue, &soundManager, &SoundManager::playScoreCue);
  QObject::connect(
    &engineAdapter, &EngineAdapter::audioPlayCrash, &soundManager, &SoundManager::playCrash);
  QObject::connect(
    &engineAdapter, &EngineAdapter::audioStartMusic, &soundManager, &SoundManager::startMusic);
  QObject::connect(
    &engineAdapter, &EngineAdapter::audioStopMusic, &soundManager, &SoundManager::stopMusic);
  QObject::connect(
    &engineAdapter, &EngineAdapter::audioDuckMusic, &soundManager, &SoundManager::duckMusic);
  QObject::connect(
    &engineAdapter, &EngineAdapter::audioSetPaused, &soundManager, &SoundManager::setPaused);
  QObject::connect(&engineAdapter,
                   &EngineAdapter::audioSetMusicEnabled,
                   &soundManager,
                   &SoundManager::setMusicEnabled);
  QObject::connect(
    &engineAdapter, &EngineAdapter::audioSetVolume, &soundManager, &SoundManager::setVolume);
  QObject::connect(
    &engineAdapter, &EngineAdapter::audioSetScore, &soundManager, &SoundManager::setScore);

  QQmlApplicationEngine engine;
  qmlRegisterUncreatableType<AppState>(
    "NenoSerpent", 1, 0, "AppState", "AppState is an enum container and cannot be instantiated");
  qmlRegisterUncreatableType<PowerUpId>(
    "NenoSerpent", 1, 0, "PowerUpId", "PowerUpId is an enum container and cannot be instantiated");
  engine.rootContext()->setContextProperty("uiCommandController", &uiCommandController);
  engine.rootContext()->setContextProperty("audioSettingsViewModel", &audioSettingsViewModel);
  engine.rootContext()->setContextProperty("selectionViewModel", &selectionViewModel);
  engine.rootContext()->setContextProperty("sessionRenderViewModel", &sessionRenderViewModel);
  engine.rootContext()->setContextProperty("sessionStatusViewModel", &sessionStatusViewModel);
  engine.rootContext()->setContextProperty("themeViewModel", &themeViewModel);
  engine.rootContext()->setContextProperty("inputInjector", &inputInjectionPipe);
  engine.rootContext()->setContextProperty("runtimeLogBridge", &runtimeLogger);
  engine.rootContext()->setContextProperty("appUiMode", uiMode);
  engine.rootContext()->setContextProperty("appLogMode", appLogMode);

  using namespace Qt::StringLiterals;
  const QUrl url(u"qrc:/src/qml/main.qml"_s);

  QObject::connect(
    &engine,
    &QQmlApplicationEngine::objectCreated,
    &app,
    [url](QObject* obj, const QUrl& objUrl) -> void {
      if (!obj && url == objUrl) {
        QCoreApplication::exit(-1);
      }
    },
    Qt::QueuedConnection);

  engine.load(url);

  // Safety delay for FSM to ensure QML engine is steady
  QTimer::singleShot(200, &engineAdapter, &EngineAdapter::lazyInitState);

  return QGuiApplication::exec();
}
