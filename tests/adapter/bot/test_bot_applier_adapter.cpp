#include <QtTest/QtTest>

#include "adapter/bot/applier.h"

class BotDecisionApplierAdapterTest final : public QObject {
  Q_OBJECT

private slots:
  void appliesDirectionChoiceAndStartCallbacks();
  void doesNotEmitChoiceChangeWhenIndexUnchanged();
};

void BotDecisionApplierAdapterTest::appliesDirectionChoiceAndStartCallbacks() {
  int selectedChoice = -1;
  bool startTriggered = false;
  QPoint enqueuedDirection{-9, -9};

  nenoserpent::adapter::bot::RuntimeOutput decision{};
  decision.consumeTick = true;
  decision.enqueueDirection = QPoint(1, 0);
  decision.setChoiceIndex = 2;
  decision.triggerStart = true;

  nenoserpent::adapter::bot::DecisionApplyInput input{};
  input.decision = decision;
  input.currentChoiceIndex = 0;
  input.enqueueDirection = [&enqueuedDirection](const QPoint& direction) -> bool {
    enqueuedDirection = direction;
    return true;
  };
  input.setChoiceIndex = [&selectedChoice](const int index) { selectedChoice = index; };
  input.triggerStart = [&startTriggered]() { startTriggered = true; };

  const auto result = nenoserpent::adapter::bot::applyDecision(input);

  QVERIFY(result.consumeTick);
  QVERIFY(result.enqueuedDirection);
  QVERIFY(result.choiceChanged);
  QVERIFY(result.startTriggered);
  QCOMPARE(result.enqueueDirection.value_or(QPoint()), QPoint(1, 0));
  QCOMPARE(result.choiceIndex.value_or(-1), 2);
  QCOMPARE(enqueuedDirection, QPoint(1, 0));
  QCOMPARE(selectedChoice, 2);
  QVERIFY(startTriggered);
}

void BotDecisionApplierAdapterTest::doesNotEmitChoiceChangeWhenIndexUnchanged() {
  bool choiceSetterCalled = false;

  nenoserpent::adapter::bot::RuntimeOutput decision{};
  decision.consumeTick = false;
  decision.setChoiceIndex = 3;

  nenoserpent::adapter::bot::DecisionApplyInput input{};
  input.decision = decision;
  input.currentChoiceIndex = 3;
  input.setChoiceIndex = [&choiceSetterCalled](const int) { choiceSetterCalled = true; };

  const auto result = nenoserpent::adapter::bot::applyDecision(input);

  QVERIFY(!result.consumeTick);
  QVERIFY(!result.choiceChanged);
  QVERIFY(!choiceSetterCalled);
  QCOMPARE(result.choiceIndex.value_or(-1), 3);
}

QTEST_MAIN(BotDecisionApplierAdapterTest)
#include "test_bot_applier_adapter.moc"
