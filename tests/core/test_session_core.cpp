#include <QtTest>

#include "core/session/core.h"

// QtTest slot-based tests intentionally stay as member functions and use assertion-heavy bodies.
// NOLINTBEGIN(readability-convert-member-functions-to-static,readability-function-cognitive-complexity)
class TestSessionCore : public QObject {
  Q_OBJECT

private slots:
  void testEnqueueDirectionRejectsReverseAndConsumesInOrder();
  void testResetMethodsClearTransientAndReplayRuntime();
  void testSnapshotRoundTripRestoresStateAndBody();
  void testBodyOwnershipAndMovement();
  void testCollisionConsumesLaserObstacleAndShield();
  void testFoodAndPowerUpConsumptionMutateSessionState();
  void testSpawnMagnetAndBuffCountdownMutateCoreState();
  void testPowerUpExpiresWhenNotEaten();
  void testAdvanceSessionStepKeepsMagnetFoodConsumptionSeparate();
  void testApplyChoiceSelectionMutatesCoreBuffState();
  void testBootstrapForLevelResetsSessionAndBuildsBody();
  void testBootstrapForLevelPreservesAliasedObstacleInput();
  void testSeedPreviewStateOverwritesSessionWithPreviewState();
  void testApplyReplayTimelineConsumesMatchingFrames();
  void testCurrentTickIntervalTracksScoreAndSlowBuff();
  void testRuntimeUpdateHooksExpireBuffAndAdvanceTick();
  void testRestorePersistedSessionClearsTransientRuntimeButKeepsPersistedFields();
  void testMetaActionFacadeRoutesBootstrapAndPreviewSeeding();
  void testTickFacadeWrapsRuntimeReplayAndStepAdvancement();
};

void TestSessionCore::testEnqueueDirectionRejectsReverseAndConsumesInOrder() {
  nenoserpent::core::SessionCore core;
  core.setDirection(QPoint(0, -1));

  QVERIFY(core.enqueueDirection(QPoint(1, 0)));
  QVERIFY(!core.enqueueDirection(QPoint(-1, 0)));
  QVERIFY(core.enqueueDirection(QPoint(0, -1)));
  QVERIFY(!core.enqueueDirection(QPoint(0, 1)));
  QVERIFY(!core.enqueueDirection(QPoint(1, 0)));

  QPoint next;
  QVERIFY(core.consumeQueuedInput(next));
  QCOMPARE(next, QPoint(1, 0));
  QVERIFY(core.consumeQueuedInput(next));
  QCOMPARE(next, QPoint(0, -1));
  QVERIFY(!core.consumeQueuedInput(next));
}

void TestSessionCore::testResetMethodsClearTransientAndReplayRuntime() {
  nenoserpent::core::SessionCore core;
  auto& state = core.state();
  state.direction = {1, 0};
  state.activeBuff = 4;
  state.buffTicksRemaining = 12;
  state.buffTicksTotal = 24;
  state.shieldActive = true;
  state.powerUpPos = {5, 6};
  state.tickCounter = 33;
  state.lastRoguelikeChoiceScore = 99;
  QVERIFY(core.enqueueDirection(QPoint(0, 1)));

  core.resetTransientRuntimeState();
  QCOMPARE(state.direction, QPoint(0, -1));
  QCOMPARE(state.activeBuff, 0);
  QCOMPARE(state.buffTicksRemaining, 0);
  QCOMPARE(state.buffTicksTotal, 0);
  QCOMPARE(state.powerUpPos, QPoint(-1, -1));
  QVERIFY(!state.shieldActive);
  QVERIFY(core.inputQueue().empty());

  core.resetReplayRuntimeState();
  QCOMPARE(state.tickCounter, 0);
  QCOMPARE(state.lastRoguelikeChoiceScore, -1000);
}

void TestSessionCore::testSnapshotRoundTripRestoresStateAndBody() {
  nenoserpent::core::SessionCore core;
  auto& state = core.state();
  state.food = {7, 8};
  state.direction = {1, 0};
  state.score = 14;
  state.obstacles = {QPoint(2, 2), QPoint(3, 2)};
  QVERIFY(core.enqueueDirection(QPoint(1, 0)));

  const std::deque<QPoint> body = {QPoint(5, 5), QPoint(4, 5), QPoint(3, 5)};
  const auto snapshot = core.snapshot(body);
  QCOMPARE(snapshot.body, body);

  nenoserpent::core::SessionCore restored;
  restored.restoreSnapshot(snapshot);

  QCOMPARE(restored.state().food, QPoint(7, 8));
  QCOMPARE(restored.state().direction, QPoint(1, 0));
  QCOMPARE(restored.state().score, 14);
  QCOMPARE(restored.state().obstacles, QList<QPoint>({QPoint(2, 2), QPoint(3, 2)}));
  QVERIFY(restored.inputQueue().empty());
}

void TestSessionCore::testBodyOwnershipAndMovement() {
  nenoserpent::core::SessionCore core;
  core.setBody({QPoint(10, 10), QPoint(10, 11), QPoint(10, 12)});

  QCOMPARE(core.headPosition(), QPoint(10, 10));

  core.applyMovement(QPoint(11, 10), false);
  QCOMPARE(core.body().front(), QPoint(11, 10));
  QCOMPARE(core.body().back(), QPoint(10, 11));
  QCOMPARE(core.body().size(), std::size_t(3));

  core.applyMovement(QPoint(12, 10), true);
  QCOMPARE(core.body().front(), QPoint(12, 10));
  QCOMPARE(core.body().size(), std::size_t(4));
}

void TestSessionCore::testCollisionConsumesLaserObstacleAndShield() {
  nenoserpent::core::SessionCore core;
  core.setBody({QPoint(5, 5), QPoint(4, 5), QPoint(3, 5)});
  core.state().obstacles = {QPoint(6, 5)};
  core.state().activeBuff = static_cast<int>(nenoserpent::core::BuffId::Laser);

  const auto laserOutcome = core.checkCollision(QPoint(6, 5), 20, 18);
  QVERIFY(!laserOutcome.collision);
  QVERIFY(laserOutcome.consumeLaser);
  QCOMPARE(core.state().obstacles.size(), 0);
  QCOMPARE(core.state().activeBuff, 0);

  core.state().shieldActive = true;
  const auto shieldOutcome = core.checkCollision(QPoint(4, 5), 20, 18);
  QVERIFY(!shieldOutcome.collision);
  QVERIFY(shieldOutcome.consumeShield);
  QVERIFY(!core.state().shieldActive);
}

void TestSessionCore::testFoodAndPowerUpConsumptionMutateSessionState() {
  nenoserpent::core::SessionCore core;
  core.setBody({QPoint(10, 10), QPoint(10, 11), QPoint(10, 12), QPoint(10, 13)});
  core.state().food = QPoint(11, 10);
  core.state().powerUpPos = QPoint(12, 10);
  core.state().powerUpType = static_cast<int>(nenoserpent::core::BuffId::Mini);
  core.state().score = 9;

  const auto foodResult = core.consumeFood(QPoint(11, 10), 20, 18, [](int) { return 0; });
  QVERIFY(foodResult.ate);
  QCOMPARE(core.state().score, foodResult.newScore);

  const auto powerResult = core.consumePowerUp(QPoint(12, 10), 40, true);
  QVERIFY(powerResult.ate);
  QVERIFY(powerResult.miniApplied);
  QCOMPARE(core.state().powerUpPos, QPoint(-1, -1));
  QCOMPARE(core.body().size(), std::size_t(3));
}

void TestSessionCore::testSpawnMagnetAndBuffCountdownMutateCoreState() {
  nenoserpent::core::SessionCore core;
  core.setBody({QPoint(10, 10), QPoint(10, 11), QPoint(10, 12)});
  core.state().obstacles = {QPoint(3, 3)};

  QVERIFY(core.spawnFood(20, 18, [](int size) {
    Q_UNUSED(size);
    return 0;
  }));
  QCOMPARE(core.state().food, QPoint(0, 0));

  QVERIFY(core.spawnPowerUp(20, 18, [](int size) {
    Q_UNUSED(size);
    return 0;
  }));
  QCOMPARE(core.state().powerUpPos, QPoint(0, 1));
  QCOMPARE(core.state().powerUpType, static_cast<int>(nenoserpent::core::BuffId::Ghost));

  core.state().activeBuff = static_cast<int>(nenoserpent::core::BuffId::Magnet);
  core.state().food = QPoint(12, 10);
  const auto magnetResult = core.applyMagnetAttraction(20, 18);
  QVERIFY(magnetResult.moved);
  QCOMPARE(core.state().food, magnetResult.newFood);

  core.state().activeBuff = static_cast<int>(nenoserpent::core::BuffId::Shield);
  core.state().buffTicksRemaining = 2;
  core.state().buffTicksTotal = 8;
  core.state().shieldActive = true;
  QVERIFY(!core.beginRuntimeUpdate().buffExpired);
  QCOMPARE(core.state().buffTicksRemaining, 1);
}

void TestSessionCore::testPowerUpExpiresWhenNotEaten() {
  nenoserpent::core::SessionCore core;
  core.setBody({QPoint(10, 10), QPoint(10, 11), QPoint(10, 12)});
  core.state().powerUpPos = QPoint(12, 10);
  core.state().powerUpType = static_cast<int>(nenoserpent::core::BuffId::Laser);
  core.state().powerUpTicksRemaining = 1;

  const auto beginResult = core.beginRuntimeUpdate();
  QVERIFY(beginResult.powerUpExpired);
  QCOMPARE(core.state().powerUpPos, QPoint(-1, -1));
  QCOMPARE(core.state().powerUpType, 0);
  QCOMPARE(core.state().powerUpTicksRemaining, 0);
}

void TestSessionCore::testAdvanceSessionStepKeepsMagnetFoodConsumptionSeparate() {
  nenoserpent::core::SessionCore core;
  core.setBody({QPoint(10, 10), QPoint(10, 11), QPoint(10, 12)});
  core.setDirection(QPoint(1, 0));
  core.state().activeBuff = static_cast<int>(nenoserpent::core::BuffId::Magnet);
  core.state().food = QPoint(12, 10);

  const auto result = core.advanceSessionStep(
    {
      .boardWidth = 20,
      .boardHeight = 18,
      .consumeInputQueue = false,
      .pauseOnChoiceTrigger = true,
    },
    [](int upperBound) {
      Q_UNUSED(upperBound);
      return 99;
    });

  QVERIFY(result.appliedMovement);
  QVERIFY(!result.ateFood);
  QVERIFY(result.magnetAteFood);
  QCOMPARE(core.state().score, 1);
  QCOMPARE(core.headPosition(), QPoint(11, 10));
}

void TestSessionCore::testApplyChoiceSelectionMutatesCoreBuffState() {
  nenoserpent::core::SessionCore core;
  core.setBody({QPoint(10, 10), QPoint(10, 11), QPoint(10, 12), QPoint(10, 13)});
  core.state().score = 18;
  core.state().lastRoguelikeChoiceScore = -1000;

  const auto result =
    core.applyChoiceSelection(static_cast<int>(nenoserpent::core::BuffId::Mini), 80, false);

  QVERIFY(result.ate);
  QVERIFY(result.miniApplied);
  QCOMPARE(core.state().lastRoguelikeChoiceScore, 18);
  QCOMPARE(core.state().activeBuff, static_cast<int>(nenoserpent::core::BuffId::None));
  QCOMPARE(core.state().buffTicksRemaining, 80);
  QCOMPARE(core.state().buffTicksTotal, 80);
  QCOMPARE(core.body().size(), std::size_t(3));
}

void TestSessionCore::testBootstrapForLevelResetsSessionAndBuildsBody() {
  nenoserpent::core::SessionCore core;
  core.state().score = 99;
  core.state().food = QPoint(7, 7);
  core.state().activeBuff = static_cast<int>(nenoserpent::core::BuffId::Shield);
  core.state().tickCounter = 12;
  QVERIFY(core.enqueueDirection(QPoint(1, 0)));

  const QList<QPoint> obstacles{QPoint(10, 10), QPoint(10, 11), QPoint(5, 5)};
  core.bootstrapForLevel(obstacles, 20, 18);

  QCOMPARE(core.state().score, 0);
  QCOMPARE(core.state().direction, QPoint(0, -1));
  QCOMPARE(core.state().activeBuff, 0);
  QCOMPARE(core.state().tickCounter, 0);
  QCOMPARE(core.state().lastRoguelikeChoiceScore, -1000);
  QCOMPARE(core.state().obstacles, obstacles);
  QVERIFY(core.inputQueue().empty());
  QCOMPARE(core.body().size(), std::size_t(3));
  for (const auto& segment : core.body()) {
    QVERIFY(!obstacles.contains(segment));
  }
}

void TestSessionCore::testBootstrapForLevelPreservesAliasedObstacleInput() {
  nenoserpent::core::SessionCore core;
  core.state().obstacles = {QPoint(2, 2), QPoint(3, 2)};

  const QList<QPoint>& aliasedObstacles = core.state().obstacles;
  core.bootstrapForLevel(aliasedObstacles, 20, 18);

  QCOMPARE(core.state().obstacles, QList<QPoint>({QPoint(2, 2), QPoint(3, 2)}));
}

void TestSessionCore::testSeedPreviewStateOverwritesSessionWithPreviewState() {
  nenoserpent::core::SessionCore core;
  core.state().lastRoguelikeChoiceScore = 88;
  QVERIFY(core.enqueueDirection(QPoint(1, 0)));

  core.seedPreviewState({
    .obstacles = {QPoint(1, 1), QPoint(2, 1)},
    .body = {QPoint(10, 4), QPoint(10, 5), QPoint(10, 6), QPoint(10, 7)},
    .food = QPoint(12, 7),
    .direction = QPoint(0, -1),
    .powerUpPos = QPoint(-1, -1),
    .powerUpType = 0,
    .score = 42,
    .tickCounter = 64,
    .activeBuff = static_cast<int>(nenoserpent::core::BuffId::Shield),
    .buffTicksRemaining = 92,
    .buffTicksTotal = 120,
    .shieldActive = true,
  });

  QCOMPARE(core.state().score, 42);
  QCOMPARE(core.state().tickCounter, 64);
  QCOMPARE(core.state().food, QPoint(12, 7));
  QCOMPARE(core.state().direction, QPoint(0, -1));
  QCOMPARE(core.state().obstacles, QList<QPoint>({QPoint(1, 1), QPoint(2, 1)}));
  QCOMPARE(core.state().activeBuff, static_cast<int>(nenoserpent::core::BuffId::Shield));
  QCOMPARE(core.state().buffTicksRemaining, 92);
  QCOMPARE(core.state().buffTicksTotal, 120);
  QVERIFY(core.state().shieldActive);
  QCOMPARE(core.state().lastRoguelikeChoiceScore, -1000);
  QCOMPARE(core.body().size(), std::size_t(4));
  QVERIFY(core.inputQueue().empty());
}

void TestSessionCore::testApplyReplayTimelineConsumesMatchingFrames() {
  nenoserpent::core::SessionCore core;
  core.state().tickCounter = 12;
  core.setDirection(QPoint(0, -1));

  int inputHistoryIndex = 0;
  int choiceHistoryIndex = 0;
  const QList<ReplayFrame> inputFrames{
    {.frame = 11, .dx = -1, .dy = 0},
    {.frame = 12, .dx = 1, .dy = 0},
    {.frame = 12, .dx = 0, .dy = 1},
    {.frame = 14, .dx = 0, .dy = -1},
  };
  const QList<ChoiceRecord> choiceFrames{
    {.frame = 10, .index = 0},
    {.frame = 12, .index = 2},
    {.frame = 15, .index = 1},
  };

  const auto result =
    core.applyReplayTimeline(inputFrames, inputHistoryIndex, choiceFrames, choiceHistoryIndex);

  QVERIFY(result.appliedInput);
  QCOMPARE(result.appliedDirection, QPoint(0, 1));
  QCOMPARE(core.direction(), QPoint(0, 1));
  QVERIFY(result.choiceIndex.has_value());
  const int choiceIndex = result.choiceIndex.value_or(-1);
  QCOMPARE(choiceIndex, 2);
  QCOMPARE(inputHistoryIndex, 3);
  QCOMPARE(choiceHistoryIndex, 2);
}

void TestSessionCore::testCurrentTickIntervalTracksScoreAndSlowBuff() {
  nenoserpent::core::SessionCore core;
  core.state().score = 25;
  QCOMPARE(core.currentTickIntervalMs(), 160);

  core.state().activeBuff = static_cast<int>(nenoserpent::core::BuffId::Slow);
  QCOMPARE(core.currentTickIntervalMs(), 250);
}

void TestSessionCore::testRuntimeUpdateHooksExpireBuffAndAdvanceTick() {
  nenoserpent::core::SessionCore core;
  core.state().tickCounter = 7;
  core.state().activeBuff = static_cast<int>(nenoserpent::core::BuffId::Shield);
  core.state().buffTicksRemaining = 1;
  core.state().buffTicksTotal = 8;
  core.state().shieldActive = true;

  const auto beginResult = core.beginRuntimeUpdate();
  QVERIFY(beginResult.buffExpired);
  QCOMPARE(core.state().activeBuff, 0);
  QCOMPARE(core.state().buffTicksRemaining, 0);
  QCOMPARE(core.state().buffTicksTotal, 0);
  QVERIFY(!core.state().shieldActive);

  core.finishRuntimeUpdate();
  QCOMPARE(core.state().tickCounter, 8);
}

void TestSessionCore::testRestorePersistedSessionClearsTransientRuntimeButKeepsPersistedFields() {
  nenoserpent::core::SessionCore core;
  const nenoserpent::core::StateSnapshot snapshot{
    .state =
      {
        .food = QPoint(4, 6),
        .powerUpPos = QPoint(8, 9),
        .powerUpType = static_cast<int>(nenoserpent::core::BuffId::Laser),
        .activeBuff = static_cast<int>(nenoserpent::core::BuffId::Shield),
        .buffTicksRemaining = 11,
        .buffTicksTotal = 18,
        .shieldActive = true,
        .direction = QPoint(1, 0),
        .score = 27,
        .obstacles = {QPoint(3, 3), QPoint(4, 3)},
        .tickCounter = 55,
        .lastRoguelikeChoiceScore = 20,
      },
    .body = {QPoint(6, 6), QPoint(5, 6), QPoint(4, 6)},
  };

  core.restorePersistedSession(snapshot);

  QCOMPARE(core.state().food, QPoint(4, 6));
  QCOMPARE(core.state().direction, QPoint(1, 0));
  QCOMPARE(core.state().score, 27);
  QCOMPARE(core.state().obstacles, QList<QPoint>({QPoint(3, 3), QPoint(4, 3)}));
  QCOMPARE(core.state().powerUpPos, QPoint(-1, -1));
  QCOMPARE(core.state().activeBuff, 0);
  QCOMPARE(core.state().buffTicksRemaining, 0);
  QCOMPARE(core.state().buffTicksTotal, 0);
  QVERIFY(!core.state().shieldActive);
  QCOMPARE(core.state().tickCounter, 0);
  QCOMPARE(core.state().lastRoguelikeChoiceScore, -1000);
  QCOMPARE(core.body(), snapshot.body);
  QVERIFY(core.inputQueue().empty());
}

void TestSessionCore::testMetaActionFacadeRoutesBootstrapAndPreviewSeeding() {
  nenoserpent::core::SessionCore core;

  core.applyMetaAction(nenoserpent::core::MetaAction::bootstrapForLevel({QPoint(2, 2)}, 20, 18));
  QCOMPARE(core.state().obstacles, QList<QPoint>({QPoint(2, 2)}));
  QCOMPARE(core.state().direction, QPoint(0, -1));
  QCOMPARE(core.body().size(), std::size_t(3));

  core.applyMetaAction(nenoserpent::core::MetaAction::seedPreviewState({
    .obstacles = {QPoint(4, 4)},
    .body = {{10, 10}, {9, 10}, {8, 10}},
    .food = QPoint(11, 10),
    .direction = QPoint(1, 0),
    .score = 7,
    .tickCounter = 12,
  }));
  QCOMPARE(core.state().obstacles, QList<QPoint>({QPoint(4, 4)}));
  QCOMPARE(core.headPosition(), QPoint(10, 10));
  QCOMPARE(core.state().score, 7);
  QCOMPARE(core.state().tickCounter, 12);
}

void TestSessionCore::testTickFacadeWrapsRuntimeReplayAndStepAdvancement() {
  nenoserpent::core::SessionCore core;
  core.applyMetaAction(nenoserpent::core::MetaAction::seedPreviewState({
    .obstacles = {},
    .body = {{10, 10}, {9, 10}, {8, 10}},
    .food = QPoint(11, 10),
    .direction = QPoint(1, 0),
    .score = 19,
    .tickCounter = 5,
    .activeBuff = static_cast<int>(nenoserpent::core::BuffId::Shield),
    .buffTicksRemaining = 2,
    .buffTicksTotal = 8,
  }));

  QList<ReplayFrame> inputFrames{{.frame = 5, .dx = 0, .dy = 1}};
  QList<ChoiceRecord> choiceFrames{{.frame = 5, .index = 2}};
  int inputIndex = 0;
  int choiceIndex = 0;

  const auto result = core.tick(
    {
      .advanceConfig =
        {
          .boardWidth = 20,
          .boardHeight = 18,
          .consumeInputQueue = false,
          .pauseOnChoiceTrigger = false,
        },
      .replayInputFrames = &inputFrames,
      .replayInputHistoryIndex = &inputIndex,
      .replayChoiceFrames = &choiceFrames,
      .replayChoiceHistoryIndex = &choiceIndex,
      .applyRuntimeHooks = true,
    },
    [](const int upperBound) {
      Q_UNUSED(upperBound);
      return 0;
    });

  QVERIFY(!result.runtimeUpdate.buffExpired);
  QVERIFY(result.replayTimeline.appliedInput);
  QCOMPARE(result.replayTimeline.appliedDirection, QPoint(0, 1));
  QVERIFY(result.replayTimeline.choiceIndex.has_value());
  QCOMPARE(*result.replayTimeline.choiceIndex, 2);
  QVERIFY(result.step.appliedMovement);
  QCOMPARE(core.state().score, 19);
  QCOMPARE(core.state().tickCounter, 6);
}

// NOLINTEND(readability-convert-member-functions-to-static,readability-function-cognitive-complexity)
QTEST_MAIN(TestSessionCore)
#include "test_session_core.moc"
