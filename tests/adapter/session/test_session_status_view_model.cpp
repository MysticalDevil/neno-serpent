#include <QSignalSpy>
#include <QtTest/QtTest>

#include "adapter/engine_adapter.h"
#include "adapter/view_models/status.h"

class TestSessionStatusViewModel : public QObject {
  Q_OBJECT

private slots:
  static void testMirrorsSessionStatusProperties();
  static void testHasSaveSignalTracksAdapter();
  static void testLevelSignalTracksAdapter();
};

void TestSessionStatusViewModel::testMirrorsSessionStatusProperties() {
  EngineAdapter engineAdapter;
  SessionStatusViewModel sessionStatusViewModel(&engineAdapter);

  QCOMPARE(sessionStatusViewModel.hasSave(), engineAdapter.hasSave());
  QCOMPARE(sessionStatusViewModel.hasReplay(), engineAdapter.hasReplay());
  QCOMPARE(sessionStatusViewModel.highScore(), engineAdapter.highScore());
  QCOMPARE(sessionStatusViewModel.level(), engineAdapter.level());
  QCOMPARE(sessionStatusViewModel.currentLevelName(), engineAdapter.currentLevelName());
}

void TestSessionStatusViewModel::testHasSaveSignalTracksAdapter() {
  EngineAdapter engineAdapter;
  SessionStatusViewModel sessionStatusViewModel(&engineAdapter);
  QSignalSpy hasSaveSpy(&sessionStatusViewModel, &SessionStatusViewModel::hasSaveChanged);

  engineAdapter.startGame();
  engineAdapter.quitToMenu();

  QVERIFY(hasSaveSpy.count() >= 1);
  QVERIFY(sessionStatusViewModel.hasSave());
}

void TestSessionStatusViewModel::testLevelSignalTracksAdapter() {
  EngineAdapter engineAdapter;
  SessionStatusViewModel sessionStatusViewModel(&engineAdapter);
  QSignalSpy levelSpy(&sessionStatusViewModel, &SessionStatusViewModel::levelChanged);

  const auto before = sessionStatusViewModel.currentLevelName();
  engineAdapter.nextLevel();

  QCOMPARE(levelSpy.count(), 1);
  QVERIFY(sessionStatusViewModel.currentLevelName() != before);
}

QTEST_MAIN(TestSessionStatusViewModel)
#include "test_session_status_view_model.moc"
