#include <QtTest/QtTest>

#include "adapter/bot/controller.h"

class BotControllerAdapterTest final : public QObject {
  Q_OBJECT

private slots:
  void avoidsImmediateCollisionWhenChoosingDirection();
  void picksShieldOverOtherChoices();
  void respectsCustomChoicePriorityFromStrategy();
};

void BotControllerAdapterTest::avoidsImmediateCollisionWhenChoosingDirection() {
  const auto direction = nenoserpent::adapter::bot::pickDirection({
    .head = QPoint(10, 10),
    .direction = QPoint(0, -1),
    .food = QPoint(10, 5),
    .boardWidth = 20,
    .boardHeight = 18,
    .obstacles = {QPoint(10, 9)},
    .body = {QPoint(10, 10), QPoint(10, 11), QPoint(10, 12)},
  });

  QVERIFY(direction.has_value());
  QVERIFY(*direction != QPoint(0, -1));
}

void BotControllerAdapterTest::picksShieldOverOtherChoices() {
  const QVariantList choices = {
    QVariantMap{{"type", 3}, {"name", "Magnet"}},
    QVariantMap{{"type", 4}, {"name", "Shield"}},
    QVariantMap{{"type", 7}, {"name", "Diamond"}},
  };

  QCOMPARE(nenoserpent::adapter::bot::pickChoiceIndex(choices), 1);
}

void BotControllerAdapterTest::respectsCustomChoicePriorityFromStrategy() {
  const QVariantList choices = {
    QVariantMap{{"type", 4}, {"name", "Shield"}},
    QVariantMap{{"type", 7}, {"name", "Diamond"}},
  };
  auto strategy = nenoserpent::adapter::bot::defaultStrategyConfig();
  strategy.powerPriorityByType.insert(4, 5);
  strategy.powerPriorityByType.insert(7, 80);

  QCOMPARE(nenoserpent::adapter::bot::pickChoiceIndex(choices, strategy), 1);
}

QTEST_MAIN(BotControllerAdapterTest)
#include "test_bot_controller_adapter.moc"
