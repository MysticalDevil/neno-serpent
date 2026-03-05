#include <QtTest/QtTest>

#include "adapter/level/script_runtime.h"

class TestLevelScriptRuntimeAdapter : public QObject {
  Q_OBJECT

private slots:
  void testTryApplyOnTickScriptParsesObstacleArray();
  void testTryApplyOnTickScriptRejectsMissingOrInvalidOnTick();
  void testApplyDynamicLevelFallbackDelegatesToCoreDynamicLevels();
};

void TestLevelScriptRuntimeAdapter::testTryApplyOnTickScriptParsesObstacleArray() {
  QJSEngine engine;
  engine.evaluate(QStringLiteral("function onTick(t){ return [{x:t,y:1},{x:2,y:t+1}]; }"));

  QList<QPoint> obstacles;
  QVERIFY(nenoserpent::adapter::tryApplyOnTickScript(engine, 7, obstacles));
  QCOMPARE(obstacles.size(), 2);
  QCOMPARE(obstacles[0], QPoint(7, 1));
  QCOMPARE(obstacles[1], QPoint(2, 8));
}

void TestLevelScriptRuntimeAdapter::testTryApplyOnTickScriptRejectsMissingOrInvalidOnTick() {
  QJSEngine engineNoTick;
  QList<QPoint> obstacles;
  QVERIFY(!nenoserpent::adapter::tryApplyOnTickScript(engineNoTick, 1, obstacles));

  QJSEngine engineInvalid;
  engineInvalid.evaluate(QStringLiteral("function onTick(t){ return 42; }"));
  QVERIFY(!nenoserpent::adapter::tryApplyOnTickScript(engineInvalid, 1, obstacles));
}

void TestLevelScriptRuntimeAdapter::testApplyDynamicLevelFallbackDelegatesToCoreDynamicLevels() {
  QList<QPoint> obstacles;
  QVERIFY(
    nenoserpent::adapter::applyDynamicLevelFallback(QStringLiteral("Dynamic Pulse"), 10, obstacles));
  QVERIFY(!obstacles.isEmpty());

  obstacles.clear();
  QVERIFY(!nenoserpent::adapter::applyDynamicLevelFallback(QStringLiteral("Classic"), 10, obstacles));
  QVERIFY(obstacles.isEmpty());
}

QTEST_MAIN(TestLevelScriptRuntimeAdapter)
#include "test_level_script_runtime_adapter.moc"
