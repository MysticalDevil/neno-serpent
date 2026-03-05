#include <QSignalSpy>
#include <QtTest/QtTest>

#include "adapter/engine_adapter.h"
#include "adapter/view_models/render.h"

class TestSessionRenderViewModel : public QObject {
  Q_OBJECT

private slots:
  static void testMirrorsRuntimeRenderProperties();
  static void testScoreSignalTracksAdapter();
  static void testBuffSignalTracksAdapter();
};

void TestSessionRenderViewModel::testMirrorsRuntimeRenderProperties() {
  EngineAdapter engineAdapter;
  SessionRenderViewModel sessionRenderViewModel(&engineAdapter);

  QCOMPARE(sessionRenderViewModel.snakeModel(), engineAdapter.snakeModelPtr());
  QCOMPARE(sessionRenderViewModel.food(), engineAdapter.food());
  QCOMPARE(sessionRenderViewModel.powerUpPos(), engineAdapter.powerUpPos());
  QCOMPARE(sessionRenderViewModel.powerUpType(), engineAdapter.powerUpType());
  QCOMPARE(sessionRenderViewModel.score(), engineAdapter.score());
  QCOMPARE(sessionRenderViewModel.highScore(), engineAdapter.highScore());
  QCOMPARE(sessionRenderViewModel.state(), engineAdapter.state());
  QCOMPARE(sessionRenderViewModel.boardWidth(), engineAdapter.boardWidth());
  QCOMPARE(sessionRenderViewModel.boardHeight(), engineAdapter.boardHeight());
  QCOMPARE(sessionRenderViewModel.obstacles(), engineAdapter.obstacles());
  QCOMPARE(sessionRenderViewModel.ghost(), engineAdapter.ghost());
  QCOMPARE(sessionRenderViewModel.activeBuff(), engineAdapter.activeBuff());
  QCOMPARE(sessionRenderViewModel.buffTicksRemaining(), engineAdapter.buffTicksRemaining());
  QCOMPARE(sessionRenderViewModel.buffTicksTotal(), engineAdapter.buffTicksTotal());
  QCOMPARE(sessionRenderViewModel.reflectionOffset(), engineAdapter.reflectionOffset());
  QCOMPARE(sessionRenderViewModel.shieldActive(), engineAdapter.shieldActive());
}

void TestSessionRenderViewModel::testScoreSignalTracksAdapter() {
  EngineAdapter engineAdapter;
  SessionRenderViewModel sessionRenderViewModel(&engineAdapter);
  QSignalSpy scoreSpy(&sessionRenderViewModel, &SessionRenderViewModel::scoreChanged);

  engineAdapter.startGame();

  QVERIFY(scoreSpy.count() >= 1);
  QCOMPARE(sessionRenderViewModel.score(), engineAdapter.score());
}

void TestSessionRenderViewModel::testBuffSignalTracksAdapter() {
  EngineAdapter engineAdapter;
  SessionRenderViewModel sessionRenderViewModel(&engineAdapter);
  QSignalSpy buffSpy(&sessionRenderViewModel, &SessionRenderViewModel::buffChanged);

  engineAdapter.debugSeedReplayBuffPreview();

  QVERIFY(buffSpy.count() >= 1);
  QCOMPARE(sessionRenderViewModel.activeBuff(), engineAdapter.activeBuff());
  QCOMPARE(sessionRenderViewModel.buffTicksRemaining(), engineAdapter.buffTicksRemaining());
  QCOMPARE(sessionRenderViewModel.shieldActive(), engineAdapter.shieldActive());
}

QTEST_MAIN(TestSessionRenderViewModel)
#include "test_session_render_view_model.moc"
