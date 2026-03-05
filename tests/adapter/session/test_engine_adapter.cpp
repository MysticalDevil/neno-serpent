#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QSettings>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QtTest>

#include "adapter/engine_adapter.h"
#include "adapter/ghost/store.h"
#include "app_state.h"
#include "power_up_id.h"

class TestEngineAdapter : public QObject {
  Q_OBJECT
private:
  class ScopedEnv final {
  public:
    explicit ScopedEnv(const char* key)
        : m_key(key),
          m_hadValue(qEnvironmentVariableIsSet(key)),
          m_previous(m_hadValue ? qgetenv(key) : QByteArray()) {
    }
    ~ScopedEnv() {
      if (m_hadValue) {
        qputenv(m_key.constData(), m_previous);
      } else {
        qunsetenv(m_key.constData());
      }
    }

  private:
    QByteArray m_key;
    bool m_hadValue = false;
    QByteArray m_previous;
  };

  static void clearPersistentState() {
    QSettings settings;
    const QString settingsFile = settings.fileName();
    settings.clear();
    settings.sync();

    if (!settingsFile.isEmpty()) {
      QFile::remove(settingsFile);
      const QFileInfo settingsInfo(settingsFile);
      QDir settingsDir(settingsInfo.absolutePath());
      settingsDir.removeRecursively();
    }

    const QString appDataDirectory =
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QFile::remove(nenoserpent::adapter::ghostFilePathForDirectory(appDataDirectory));
    QDir appDataDir(appDataDirectory);
    if (appDataDir.exists()) {
      appDataDir.removeRecursively();
    }
  }

  static auto pickBuff(EngineAdapter& game, int buffType) -> bool {
    for (int attempt = 0; attempt < 80; ++attempt) {
      game.generateChoices();
      const auto currentChoices = game.choices();
      for (int i = 0; i < currentChoices.size(); ++i) {
        const auto map = currentChoices[i].toMap();
        if (map.value("type").toInt() == buffType) {
          game.selectChoice(i);
          return true;
        }
      }
    }
    return false;
  }

  static void consumeCurrentFood(EngineAdapter& game) {
    const int previousScore = game.score();

    for (int attempt = 0; attempt < 4; ++attempt) {
      const QPoint food = game.food();

      QPoint direction;
      std::deque<QPoint> body;
      if ((attempt % 2) == 0) {
        if (food.x() >= 3) {
          direction = QPoint(1, 0);
          body = {
            QPoint(food.x() - 1, food.y()),
            QPoint(food.x() - 2, food.y()),
            QPoint(food.x() - 3, food.y()),
          };
        } else {
          direction = QPoint(-1, 0);
          body = {
            QPoint(food.x() + 1, food.y()),
            QPoint(food.x() + 2, food.y()),
            QPoint(food.x() + 3, food.y()),
          };
        }
      } else {
        if (food.y() >= 3) {
          direction = QPoint(0, 1);
          body = {
            QPoint(food.x(), food.y() - 1),
            QPoint(food.x(), food.y() - 2),
            QPoint(food.x(), food.y() - 3),
          };
        } else {
          direction = QPoint(0, -1);
          body = {
            QPoint(food.x(), food.y() + 1),
            QPoint(food.x(), food.y() + 2),
            QPoint(food.x(), food.y() + 3),
          };
        }
      }

      game.snakeModelPtr()->reset(body);
      game.setDirection(direction);
      game.forceUpdate();

      if (game.score() > previousScore || game.state() == AppState::ChoiceSelection) {
        return;
      }
    }

    QFAIL("Failed to consume the current food through the active session-step path");
  }

private slots:
  void initTestCase() {
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setOrganizationName("NenoSerpentTests");
    QCoreApplication::setApplicationName("EngineAdapterTest");
    clearPersistentState();
  }

  void init() {
    clearPersistentState();
  }

  void cleanup() {
    clearPersistentState();
  }

  void testInitialState() {
    EngineAdapter game;
    QVERIFY(game.state() == AppState::Splash || game.state() == AppState::StartMenu);
  }

  void testGameCycle() {
    EngineAdapter game;
    game.startGame();

    // Push logic far enough to ensure it hits boundary or completes splash
    for (int i = 0; i < 100; ++i)
      game.forceUpdate();

    // After 100 forced steps, it MUST have transitioned out of Playing or hit boundary
    QVERIFY(game.state() != AppState::Splash);
    QVERIFY(game.state() == AppState::Playing || game.state() == AppState::GameOver);
  }

  void testSnakeMovesAfterStart() {
    EngineAdapter game;
    game.startGame();
    const QPoint before = game.snakeModelPtr()->body().front();
    game.forceUpdate();
    const QPoint after = game.snakeModelPtr()->body().front();
    QVERIFY(before != after);
  }

  void testSplashTransitionRequest() {
    EngineAdapter game;
    game.requestStateChange(AppState::StartMenu);
    QVERIFY(game.state() == AppState::StartMenu);
    game.requestStateChange(AppState::Splash);
    QVERIFY(game.state() == AppState::Splash);
  }

  void testNextLevelChangesName() {
    EngineAdapter game;
    const QString before = game.currentLevelName();
    game.nextLevel();
    const QString after = game.currentLevelName();
    QVERIFY(before != after);
  }

  void testHandleSelectInMenuChangesLevel() {
    EngineAdapter game;
    game.requestStateChange(AppState::StartMenu);
    const QString before = game.currentLevelName();
    game.handleSelect();
    const QString after = game.currentLevelName();
    QVERIFY(before != after);
  }

  void testTheCageAppliesObstaclesOnNewRun() {
    EngineAdapter game;
    game.requestStateChange(AppState::StartMenu);
    game.deleteSave();
    QVERIFY2(!game.hasSave(), "Save session should be cleared before starting The Cage");

    // Classic -> The Cage
    game.handleSelect();
    QCOMPARE(game.currentLevelName(), QString("The Cage"));

    game.handleStart();
    QVERIFY(game.state() == AppState::Playing || game.state() == AppState::Paused);
    QVERIFY2(game.obstacles().size() > 0, "The Cage should spawn wall obstacles in-game");
  }

  void testDynamicPulseHasObstaclesOnStart() {
    EngineAdapter game;
    game.requestStateChange(AppState::StartMenu);
    game.deleteSave();

    // Classic -> The Cage -> Dynamic Pulse
    game.handleSelect();
    game.handleSelect();
    QCOMPARE(game.currentLevelName(), QString("Dynamic Pulse"));

    game.handleStart();
    QVERIFY(game.state() == AppState::Playing || game.state() == AppState::Paused);
    QVERIFY2(game.obstacles().size() > 0, "Dynamic Pulse should produce dynamic obstacles");
  }

  void testDynamicPulseObstaclesMoveOverTime() {
    EngineAdapter game;
    game.requestStateChange(AppState::StartMenu);
    game.deleteSave();

    game.handleSelect();
    game.handleSelect();
    QCOMPARE(game.currentLevelName(), QString("Dynamic Pulse"));

    game.handleStart();
    const QVariantList before = game.obstacles();
    QVariantList after = before;
    for (int i = 0; i < 180 && after == before; ++i) {
      game.forceUpdate();
      after = game.obstacles();
    }
    QVERIFY2(before != after, "Dynamic Pulse obstacles should change over time");
  }

  void testPausedDynamicPulseFreezesObstacles() {
    EngineAdapter game;
    game.requestStateChange(AppState::StartMenu);
    game.deleteSave();

    game.handleSelect();
    game.handleSelect();
    QCOMPARE(game.currentLevelName(), QString("Dynamic Pulse"));

    game.handleStart();
    QCOMPARE(game.state(), AppState::Playing);

    game.handleStart();
    QCOMPARE(game.state(), AppState::Paused);

    const QVariantList before = game.obstacles();
    const int tickBefore = game.currentTick();
    for (int i = 0; i < 12; ++i) {
      game.forceUpdate();
    }
    const QVariantList after = game.obstacles();
    QCOMPARE(before, after);
    QCOMPARE(game.currentTick(), tickBefore);
  }

  void testDeleteSaveResetsLevelToClassicAndNewRunUsesClassic() {
    EngineAdapter game;
    game.requestStateChange(AppState::StartMenu);
    game.deleteSave();

    // Move away from Classic and create a save by returning to menu.
    game.handleSelect();
    game.handleSelect();
    QCOMPARE(game.currentLevelName(), QString("Dynamic Pulse"));
    game.handleStart();
    QVERIFY(game.state() == AppState::Playing || game.state() == AppState::Paused);
    game.quitToMenu();
    QVERIFY2(game.hasSave(), "Session save should exist after quitting from an active run");

    // Clear save and start without selecting level again.
    game.deleteSave();
    QVERIFY2(!game.hasSave(), "Session save should be cleared");
    QCOMPARE(game.currentLevelName(), QString("Classic"));

    game.handleStart();
    QVERIFY2(game.state() == AppState::Playing || game.state() == AppState::Paused,
             "Start should begin a new run, not reload old session");
    QCOMPARE(game.obstacles().size(), 0);
  }

  void testDeleteSaveThenSelectStartsChosenLevel() {
    EngineAdapter game;
    game.requestStateChange(AppState::StartMenu);
    game.deleteSave();
    QCOMPARE(game.currentLevelName(), QString("Classic"));

    // After reset to Classic, one select should choose The Cage.
    game.handleSelect();
    QCOMPARE(game.currentLevelName(), QString("The Cage"));

    game.handleStart();
    QVERIFY2(game.state() == AppState::Playing || game.state() == AppState::Paused,
             "Start should begin a new run");
    QVERIFY2(game.obstacles().size() > 0, "The Cage run should use The Cage obstacles");
  }

  void testTunnelRunInitialSpawnAvoidsObstacles() {
    EngineAdapter game;
    game.requestStateChange(AppState::StartMenu);
    game.deleteSave();

    // Classic -> The Cage -> Dynamic Pulse -> Tunnel Run
    game.handleSelect();
    game.handleSelect();
    game.handleSelect();
    QCOMPARE(game.currentLevelName(), QString("Tunnel Run"));

    game.handleStart();
    QVERIFY(game.state() == AppState::Playing || game.state() == AppState::Paused);

    QSet<QPoint> obstacleSet;
    for (const QVariant& item : game.obstacles()) {
      const QVariantMap map = item.toMap();
      obstacleSet.insert(QPoint(map.value("x").toInt(), map.value("y").toInt()));
    }
    for (const QPoint& segment : game.snakeModelPtr()->body()) {
      QVERIFY2(!obstacleSet.contains(segment),
               "Tunnel Run initial snake body must not overlap obstacles");
    }
  }

  void testHeadWrapsAtBoundary() {
    EngineAdapter game;
    game.requestStateChange(AppState::StartMenu);
    game.deleteSave();
    game.handleStart();

    game.snakeModelPtr()->reset({QPoint(19, 10), QPoint(18, 10), QPoint(17, 10)});
    game.setDirection(QPoint(1, 0));

    game.forceUpdate();
    const QPoint head = game.snakeModelPtr()->body().front();
    QCOMPARE(head, QPoint(0, 10));
  }

  void testDoubleBuffCrossesTenThresholdKeepsValidState() {
    EngineAdapter game;
    game.startGame();

    for (int attempt = 0; attempt < 20 && game.score() < 9; ++attempt) {
      if (game.state() == AppState::ChoiceSelection) {
        game.selectChoice(0);
        continue;
      }
      consumeCurrentFood(game);
    }
    QVERIFY2(game.score() >= 9,
             "Expected score to reach the pre-threshold range before picking Double");

    QVERIFY2(pickBuff(game, PowerUpId::Double),
             "Failed to pick Double buff from generated choices");
    QCOMPARE(game.activeBuff(), static_cast<int>(PowerUpId::Double));

    const int scoreBeforeDoubleFood = game.score();
    consumeCurrentFood(game);
    QCOMPARE(game.score(), scoreBeforeDoubleFood + 2);
    QVERIFY2(
      game.state() == AppState::Playing || game.state() == AppState::ChoiceSelection,
      "Dynamic roguelike trigger should keep state valid without forcing a fixed threshold popup");
  }

  void testQuitToMenuFromGameOverDoesNotCreateSave() {
    EngineAdapter game;
    game.requestStateChange(AppState::StartMenu);
    game.deleteSave();
    QVERIFY(!game.hasSave());

    game.handleStart();
    QVERIFY(game.state() == AppState::Playing || game.state() == AppState::Paused);

    game.snakeModelPtr()->reset({QPoint(10, 10), QPoint(11, 10), QPoint(12, 10)});
    game.setDirection(QPoint(1, 0));
    game.forceUpdate();
    QCOMPARE(game.state(), AppState::GameOver);
    QVERIFY(!game.hasSave());

    game.quitToMenu();
    QCOMPARE(game.state(), AppState::StartMenu);
    QVERIFY2(!game.hasSave(), "GameOver back-to-menu should not generate a continue save");
  }

  void testLatestBuffSelectionOverridesPreviousBuff() {
    EngineAdapter game;
    game.startGame();

    QVERIFY2(pickBuff(game, PowerUpId::Ghost), "Failed to pick Ghost buff");
    QCOMPARE(game.activeBuff(), static_cast<int>(PowerUpId::Ghost));

    QVERIFY2(pickBuff(game, PowerUpId::Slow), "Failed to pick Slow buff");
    QCOMPARE(game.activeBuff(), static_cast<int>(PowerUpId::Slow));
  }

  void testReplayAppliesRecordedInputOnMatchingFrame() {
    EngineAdapter game;
    game.startGame();

    game.move(1, 0);
    game.forceUpdate();

    consumeCurrentFood(game);
    QVERIFY(game.score() > 0);

    game.snakeModelPtr()->reset({QPoint(10, 10), QPoint(11, 10), QPoint(12, 10)});
    game.setDirection(QPoint(1, 0));
    game.forceUpdate();
    QCOMPARE(game.state(), AppState::GameOver);
    QVERIFY2(game.hasReplay(), "Replay should be available after a new high score run");

    game.requestStateChange(AppState::StartMenu);
    game.startReplay();
    QCOMPARE(game.state(), AppState::Replaying);

    game.forceUpdate();
    QCOMPARE(game.snakeModelPtr()->body().front(), QPoint(11, 10));
  }

  void testCycleBotStrategyModeUpdatesStatusWithoutChangingBackend() {
    EngineAdapter game;
    const auto before = game.botStatus();
    const QString beforeBackend = before.value("backend").toString();
    const QString beforeMode = before.value("mode").toString();

    game.cycleBotStrategyMode();
    const auto after = game.botStatus();

    QCOMPARE(after.value("backend").toString(), beforeBackend);
    QVERIFY(after.value("mode").toString() != beforeMode);
    QVERIFY(after.contains("mlAvailable"));
  }

  void testBotBackendEnvironmentOverrideModes() {
    const auto checkBackend = [](const char* mode, const char* expected) {
      ScopedEnv scopedBackend("NENOSERPENT_BOT_BACKEND");
      qputenv("NENOSERPENT_BOT_BACKEND", mode);
      EngineAdapter game;
      QCOMPARE(game.botStatus().value("backend").toString(), QString::fromLatin1(expected));
    };

    checkBackend("off", "off");
    checkBackend("rule", "rule");
    checkBackend("ml", "ml");
    checkBackend("search", "search");
  }

  void testInvalidBotBackendOverrideFallsBackToOff() {
    ScopedEnv scopedBackend("NENOSERPENT_BOT_BACKEND");
    qputenv("NENOSERPENT_BOT_BACKEND", "unknown-backend");

    EngineAdapter game;
    QCOMPARE(game.botStatus().value("backend").toString(), QString("off"));
  }

  void testEmitAudioEventRoutesAllVariants() {
    EngineAdapter game;
    QSignalSpy foodSpy(&game, &EngineAdapter::foodEaten);
    QSignalSpy crashSpy(&game, &EngineAdapter::playerCrashed);
    QSignalSpy uiSpy(&game, &EngineAdapter::uiInteractTriggered);
    QSignalSpy powerSpy(&game, &EngineAdapter::powerUpEaten);

    game.emitAudioEvent(nenoserpent::audio::Event::Food, 0.25F);
    game.emitAudioEvent(nenoserpent::audio::Event::Crash, 0.0F);
    game.emitAudioEvent(nenoserpent::audio::Event::UiInteract, 0.0F);
    game.emitAudioEvent(nenoserpent::audio::Event::Confirm, 0.0F);
    game.emitAudioEvent(nenoserpent::audio::Event::PowerUp, 0.0F);

    QCOMPARE(foodSpy.count(), 1);
    QCOMPARE(crashSpy.count(), 1);
    QCOMPARE(uiSpy.count(), 1);
    QCOMPARE(powerSpy.count(), 1);
  }
};

QTEST_MAIN(TestEngineAdapter)
#include "test_engine_adapter.moc"
