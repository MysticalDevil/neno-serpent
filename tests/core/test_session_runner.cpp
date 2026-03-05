#include <QtTest>

#include "core/session/runner.h"

// QtTest slot-based tests intentionally stay as member functions and use assertion-heavy bodies.
// NOLINTBEGIN(readability-convert-member-functions-to-static,readability-function-cognitive-complexity)
class TestSessionRunner : public QObject {
  Q_OBJECT

private slots:
  void testRunnerCanAdvanceHeadlessSessionAndEnterChoice();
  void testRunnerCanReplayRecordedTimelineHeadlessly();
};

void TestSessionRunner::testRunnerCanAdvanceHeadlessSessionAndEnterChoice() {
  nenoserpent::core::SessionRunner runner;
  runner.seedPreviewState(
    {
      .obstacles = {},
      .body = {{10, 10}, {9, 10}, {8, 10}},
      .food = QPoint(11, 10),
      .direction = QPoint(1, 0),
      .powerUpPos = QPoint(-1, -1),
      .powerUpType = 0,
      .score = 19,
    },
    nenoserpent::core::SessionMode::Playing,
    1234U);

  const auto tickResult = runner.tick();
  QVERIFY(tickResult.enteredChoice);
  QCOMPARE(runner.mode(), nenoserpent::core::SessionMode::ChoiceSelection);
  QCOMPARE(runner.core().state().score, 20);
  QCOMPARE(runner.choices().size(), 3);
  QCOMPARE(runner.recording().size(), 0);

  QVERIFY(runner.selectChoice(1));
  QCOMPARE(runner.mode(), nenoserpent::core::SessionMode::Playing);
  QCOMPARE(runner.choiceHistory().size(), 1);
  QCOMPARE(runner.choiceHistory().front().frame, 1);
}

void TestSessionRunner::testRunnerCanReplayRecordedTimelineHeadlessly() {
  const nenoserpent::core::PreviewSeed seed{
    .obstacles = {},
    .body = {{10, 10}, {9, 10}, {8, 10}},
    .food = QPoint(11, 10),
    .direction = QPoint(1, 0),
    .powerUpPos = QPoint(-1, -1),
    .powerUpType = 0,
    .score = 0,
    .tickCounter = 0,
  };

  nenoserpent::core::SessionRunner liveRunner;
  liveRunner.seedPreviewState(seed, nenoserpent::core::SessionMode::Playing, 77U);

  auto tickOne = liveRunner.tick();
  QVERIFY(tickOne.advanced);
  QCOMPARE(liveRunner.core().headPosition(), QPoint(11, 10));
  QCOMPARE(liveRunner.core().state().score, 1);

  QVERIFY(liveRunner.enqueueDirection(QPoint(0, 1)));
  auto tickTwo = liveRunner.tick();
  QVERIFY(tickTwo.advanced);
  QVERIFY(tickTwo.consumedInput);

  const auto expectedSnapshot = liveRunner.core().snapshot({});
  const auto expectedRecording = liveRunner.recording();

  nenoserpent::core::SessionRunner replayRunner;
  replayRunner.seedPreviewState(seed, nenoserpent::core::SessionMode::Replaying, 77U);
  replayRunner.setReplayTimeline(liveRunner.inputHistory(), liveRunner.choiceHistory());

  auto replayTickOne = replayRunner.tick();
  QVERIFY(replayTickOne.advanced);
  auto replayTickTwo = replayRunner.tick();
  QVERIFY(replayTickTwo.advanced);

  const auto replaySnapshot = replayRunner.core().snapshot({});
  QCOMPARE(replaySnapshot.state.score, expectedSnapshot.state.score);
  QCOMPARE(replaySnapshot.state.food, expectedSnapshot.state.food);
  QCOMPARE(replaySnapshot.state.direction, expectedSnapshot.state.direction);
  QCOMPARE(replaySnapshot.body, expectedSnapshot.body);
  QCOMPARE(replayRunner.recording(), expectedRecording);
}

QTEST_MAIN(TestSessionRunner)
// NOLINTEND(readability-convert-member-functions-to-static,readability-function-cognitive-complexity)
#include "test_session_runner.moc"
