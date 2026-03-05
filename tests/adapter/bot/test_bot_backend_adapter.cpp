#include <QtTest/QtTest>

#include "adapter/bot/backend.h"

class BotBackendAdapterTest final : public QObject {
  Q_OBJECT

private slots:
  void searchBackendRejectsInvalidSnapshot();
  void searchBackendChoosesNonReverseSafeDirection();
  void searchBackendReturnsNulloptWhenNoValidMove();
  void searchBackendPrefersHighPriorityPowerTarget();
  void backendChoiceSelectionUsesCommonPriorityLogic();
};

void BotBackendAdapterTest::searchBackendRejectsInvalidSnapshot() {
  const auto& backend = nenoserpent::adapter::bot::searchBackend();

  nenoserpent::adapter::bot::Snapshot emptyBody{};
  emptyBody.body.clear();
  QVERIFY(!backend.decideDirection(emptyBody, nenoserpent::adapter::bot::defaultStrategyConfig())
             .has_value());

  nenoserpent::adapter::bot::Snapshot invalidBoard{};
  invalidBoard.body = {QPoint(0, 0)};
  invalidBoard.boardWidth = 0;
  QVERIFY(!backend.decideDirection(invalidBoard, nenoserpent::adapter::bot::defaultStrategyConfig())
             .has_value());
}

void BotBackendAdapterTest::searchBackendChoosesNonReverseSafeDirection() {
  const auto& backend = nenoserpent::adapter::bot::searchBackend();

  nenoserpent::adapter::bot::Snapshot snapshot{};
  snapshot.boardWidth = 6;
  snapshot.boardHeight = 6;
  snapshot.head = QPoint(2, 2);
  snapshot.direction = QPoint(0, -1);
  snapshot.food = QPoint(1, 2);
  snapshot.body = {QPoint(2, 2), QPoint(2, 3), QPoint(2, 4)};
  snapshot.obstacles = {QPoint(2, 1)};

  const auto direction =
    backend.decideDirection(snapshot, nenoserpent::adapter::bot::defaultStrategyConfig());
  QVERIFY(direction.has_value());
  QCOMPARE(*direction, QPoint(-1, 0));
}

void BotBackendAdapterTest::searchBackendReturnsNulloptWhenNoValidMove() {
  const auto& backend = nenoserpent::adapter::bot::searchBackend();

  nenoserpent::adapter::bot::Snapshot snapshot{};
  snapshot.boardWidth = 5;
  snapshot.boardHeight = 5;
  snapshot.head = QPoint(2, 2);
  snapshot.direction = QPoint(0, -1);
  snapshot.body = {QPoint(2, 2)};
  snapshot.obstacles = {QPoint(2, 1), QPoint(2, 3), QPoint(1, 2), QPoint(3, 2)};

  const auto direction =
    backend.decideDirection(snapshot, nenoserpent::adapter::bot::defaultStrategyConfig());
  QVERIFY(!direction.has_value());
}

void BotBackendAdapterTest::searchBackendPrefersHighPriorityPowerTarget() {
  const auto& backend = nenoserpent::adapter::bot::searchBackend();

  auto strategy = nenoserpent::adapter::bot::defaultStrategyConfig();
  strategy.powerPriorityByType.insert(7, 100);
  strategy.powerTargetPriorityThreshold = 60;
  strategy.powerTargetDistanceSlack = 2;

  nenoserpent::adapter::bot::Snapshot snapshot{};
  snapshot.boardWidth = 6;
  snapshot.boardHeight = 6;
  snapshot.head = QPoint(2, 2);
  snapshot.direction = QPoint(0, -1);
  snapshot.food = QPoint(0, 2);
  snapshot.powerUpPos = QPoint(3, 2);
  snapshot.powerUpType = 7;
  snapshot.body = {QPoint(2, 2), QPoint(2, 3), QPoint(2, 4)};
  snapshot.obstacles = {QPoint(2, 1), QPoint(1, 2)};

  const auto direction = backend.decideDirection(snapshot, strategy);
  QVERIFY(direction.has_value());
  QCOMPARE(*direction, QPoint(1, 0));
}

void BotBackendAdapterTest::backendChoiceSelectionUsesCommonPriorityLogic() {
  const auto& rule = nenoserpent::adapter::bot::ruleBackend();
  const auto& search = nenoserpent::adapter::bot::searchBackend();

  auto strategy = nenoserpent::adapter::bot::defaultStrategyConfig();
  strategy.powerPriorityByType.insert(4, 5);
  strategy.powerPriorityByType.insert(7, 80);

  const QVariantList choices = {
    QVariantMap{{"type", 4}, {"name", "Shield"}},
    QVariantMap{{"type", 7}, {"name", "Diamond"}},
    QVariantMap{{"type", 3}, {"name", "Magnet"}},
  };

  QCOMPARE(rule.decideChoice(choices, strategy), 1);
  QCOMPARE(search.decideChoice(choices, strategy), 1);
}

QTEST_MAIN(BotBackendAdapterTest)
#include "test_bot_backend_adapter.moc"
