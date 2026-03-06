#include <QtTest/QtTest>

#include "adapter/bot/features.h"

class BotFeaturesAdapterTest final : public QObject {
  Q_OBJECT

private slots:
  void dangerFeaturesWrapAtBoardEdge();
  void dangerFeaturesRespectCollisionBuffs();
};

void BotFeaturesAdapterTest::dangerFeaturesWrapAtBoardEdge() {
  nenoserpent::adapter::bot::Snapshot snapshot{};
  snapshot.head = QPoint(0, 0);
  snapshot.direction = QPoint(1, 0);
  snapshot.food = QPoint(2, 2);
  snapshot.boardWidth = 5;
  snapshot.boardHeight = 5;
  snapshot.body = {QPoint(0, 0), QPoint(0, 1), QPoint(0, 2)};

  const auto features = nenoserpent::adapter::bot::extractFeatures(snapshot);
  QCOMPARE(features.values[17], 0.0F); // up wraps to y=4
  QCOMPARE(features.values[20], 0.0F); // left wraps to x=4
}

void BotFeaturesAdapterTest::dangerFeaturesRespectCollisionBuffs() {
  nenoserpent::adapter::bot::Snapshot base{};
  base.head = QPoint(0, 0);
  base.direction = QPoint(0, -1);
  base.food = QPoint(3, 3);
  base.boardWidth = 6;
  base.boardHeight = 6;
  base.obstacles = {QPoint(1, 0)};
  base.body = {QPoint(0, 0), QPoint(0, 1), QPoint(0, 2)};

  const auto baseFeatures = nenoserpent::adapter::bot::extractFeatures(base);
  QCOMPARE(baseFeatures.values[18], 1.0F); // right hits obstacle, fatal by default

  auto shieldSnapshot = base;
  shieldSnapshot.shieldActive = true;
  const auto shieldFeatures = nenoserpent::adapter::bot::extractFeatures(shieldSnapshot);
  QCOMPARE(shieldFeatures.values[18], 0.0F);

  auto portalSnapshot = base;
  portalSnapshot.portalActive = true;
  const auto portalFeatures = nenoserpent::adapter::bot::extractFeatures(portalSnapshot);
  QCOMPARE(portalFeatures.values[18], 0.0F);

  auto laserSnapshot = base;
  laserSnapshot.laserActive = true;
  const auto laserFeatures = nenoserpent::adapter::bot::extractFeatures(laserSnapshot);
  QCOMPARE(laserFeatures.values[18], 0.0F);

  auto ghostSnapshot = base;
  ghostSnapshot.obstacles.clear();
  ghostSnapshot.ghostActive = true;
  ghostSnapshot.body = {QPoint(0, 0), QPoint(0, 5), QPoint(1, 5)};
  const auto ghostFeatures = nenoserpent::adapter::bot::extractFeatures(ghostSnapshot);
  QCOMPARE(ghostFeatures.values[17], 0.0F); // up wraps to own body cell, ghost makes it non-fatal
}

QTEST_MAIN(BotFeaturesAdapterTest)
#include "test_bot_features_adapter.moc"
