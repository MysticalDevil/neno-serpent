#include <QtTest/QtTest>

#include "adapter/bot/snapshot_builder.h"
#include "core/buff/runtime.h"

class BotSnapshotBuilderAdapterTest final : public QObject {
  Q_OBJECT

private slots:
  void mapsCoreFieldsToSnapshot();
  void mapsBuffFlagsFromActiveBuff();
};

void BotSnapshotBuilderAdapterTest::mapsCoreFieldsToSnapshot() {
  const auto snapshot = nenoserpent::adapter::bot::buildSnapshot({
    .head = QPoint(10, 8),
    .direction = QPoint(1, 0),
    .food = QPoint(3, 4),
    .powerUpPos = QPoint(7, 2),
    .powerUpType = 4,
    .score = 42,
    .levelIndex = 3,
    .activeBuff = 0,
    .shieldActive = true,
    .boardWidth = 20,
    .boardHeight = 18,
    .obstacles = {QPoint(1, 1), QPoint(2, 2)},
    .body = {QPoint(10, 8), QPoint(9, 8), QPoint(8, 8)},
  });

  QCOMPARE(snapshot.head, QPoint(10, 8));
  QCOMPARE(snapshot.direction, QPoint(1, 0));
  QCOMPARE(snapshot.food, QPoint(3, 4));
  QCOMPARE(snapshot.powerUpPos, QPoint(7, 2));
  QCOMPARE(snapshot.powerUpType, 4);
  QCOMPARE(snapshot.score, 42);
  QCOMPARE(snapshot.levelIndex, 3);
  QVERIFY(snapshot.shieldActive);
  QCOMPARE(snapshot.boardWidth, 20);
  QCOMPARE(snapshot.boardHeight, 18);
  QCOMPARE(snapshot.obstacles.size(), 2);
  QCOMPARE(static_cast<int>(snapshot.body.size()), 3);
}

void BotSnapshotBuilderAdapterTest::mapsBuffFlagsFromActiveBuff() {
  const auto ghost = nenoserpent::adapter::bot::buildSnapshot({
    .activeBuff = static_cast<int>(nenoserpent::core::BuffId::Ghost),
    .obstacles = {},
    .body = {},
  });
  QVERIFY(ghost.ghostActive);
  QVERIFY(!ghost.portalActive);
  QVERIFY(!ghost.laserActive);

  const auto portal = nenoserpent::adapter::bot::buildSnapshot({
    .activeBuff = static_cast<int>(nenoserpent::core::BuffId::Portal),
    .obstacles = {},
    .body = {},
  });
  QVERIFY(!portal.ghostActive);
  QVERIFY(portal.portalActive);
  QVERIFY(!portal.laserActive);

  const auto laser = nenoserpent::adapter::bot::buildSnapshot({
    .activeBuff = static_cast<int>(nenoserpent::core::BuffId::Laser),
    .obstacles = {},
    .body = {},
  });
  QVERIFY(!laser.ghostActive);
  QVERIFY(!laser.portalActive);
  QVERIFY(laser.laserActive);
}

QTEST_MAIN(BotSnapshotBuilderAdapterTest)
#include "test_bot_snapshot_builder_adapter.moc"
