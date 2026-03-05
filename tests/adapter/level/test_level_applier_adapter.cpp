#include <QtTest/QtTest>

#include "adapter/level/applier.h"

class TestLevelApplierAdapter : public QObject {
  Q_OBJECT

private slots:
  void testApplyStaticLevelUsesWalls();
  void testApplyScriptedLevelRequiresScriptSuccessAndObstacles();
};

void TestLevelApplierAdapter::testApplyStaticLevelUsesWalls() {
  nenoserpent::core::ResolvedLevelData resolved{
    .name = QStringLiteral("Static"), .script = QString(), .walls = {QPoint(1, 2), QPoint(3, 4)}};
  QString levelName;
  QString script;
  QList<QPoint> obstacles;

  const bool ok = nenoserpent::adapter::applyResolvedLevelData(
    resolved, levelName, script, obstacles, [](const QString&) -> bool { return false; });

  QVERIFY(ok);
  QCOMPARE(levelName, QString("Static"));
  QVERIFY(script.isEmpty());
  QCOMPARE(obstacles.size(), 2);
}

void TestLevelApplierAdapter::testApplyScriptedLevelRequiresScriptSuccessAndObstacles() {
  nenoserpent::core::ResolvedLevelData resolved{.name = QStringLiteral("Scripted"),
                                            .script =
                                              QStringLiteral("function onTick(t){return [];}"),
                                            .walls = {}};
  QString levelName;
  QString script;
  QList<QPoint> obstacles;

  const bool failedEval = nenoserpent::adapter::applyResolvedLevelData(
    resolved, levelName, script, obstacles, [](const QString&) -> bool { return false; });
  QVERIFY(!failedEval);

  const bool emptyResult = nenoserpent::adapter::applyResolvedLevelData(
    resolved, levelName, script, obstacles, [](const QString&) -> bool { return true; });
  QVERIFY(!emptyResult);

  const bool ok = nenoserpent::adapter::applyResolvedLevelData(
    resolved, levelName, script, obstacles, [&obstacles](const QString&) -> bool {
      obstacles = {QPoint(7, 8)};
      return true;
    });
  QVERIFY(ok);
  QCOMPARE(levelName, QString("Scripted"));
  QVERIFY(!script.isEmpty());
  QCOMPARE(obstacles.size(), 1);
  QCOMPARE(obstacles.first(), QPoint(7, 8));
}

QTEST_MAIN(TestLevelApplierAdapter)
#include "test_level_applier_adapter.moc"
