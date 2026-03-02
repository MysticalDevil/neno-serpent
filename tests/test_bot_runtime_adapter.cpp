#include <QtTest/QtTest>

#include "adapter/bot/runtime.h"

class BotRuntimeAdapterTest final : public QObject {
  Q_OBJECT

private slots:
  void startMenuTriggersStartWhenEnabled();
  void choiceSelectionPicksChoiceAndConfirms();
  void cooldownDelaysNonPlayingActions();
  void usesCustomCooldownFromStrategy();
};

void BotRuntimeAdapterTest::startMenuTriggersStartWhenEnabled() {
  nenoserpent::adapter::bot::RuntimeInput input{};
  input.enabled = true;
  input.cooldownTicks = 0;
  input.state = AppState::StartMenu;
  const auto result = nenoserpent::adapter::bot::step(input);

  QVERIFY(result.triggerStart);
  QVERIFY(result.consumeTick);
  QCOMPARE(result.nextCooldownTicks, 4);
}

void BotRuntimeAdapterTest::choiceSelectionPicksChoiceAndConfirms() {
  const QVariantList choices = {
    QVariantMap{{"type", 3}},
    QVariantMap{{"type", 4}},
    QVariantMap{{"type", 7}},
  };

  nenoserpent::adapter::bot::RuntimeInput input{};
  input.enabled = true;
  input.cooldownTicks = 0;
  input.state = AppState::ChoiceSelection;
  input.choices = choices;
  input.currentChoiceIndex = 0;
  const auto result = nenoserpent::adapter::bot::step(input);

  QVERIFY(result.triggerStart);
  QVERIFY(result.consumeTick);
  QVERIFY(result.setChoiceIndex.has_value());
  QCOMPARE(*result.setChoiceIndex, 1);
  QCOMPARE(result.nextCooldownTicks, 2);
}

void BotRuntimeAdapterTest::cooldownDelaysNonPlayingActions() {
  nenoserpent::adapter::bot::RuntimeInput input{};
  input.enabled = true;
  input.cooldownTicks = 2;
  input.state = AppState::Paused;
  const auto result = nenoserpent::adapter::bot::step(input);

  QVERIFY(!result.triggerStart);
  QVERIFY(!result.consumeTick);
  QCOMPARE(result.nextCooldownTicks, 1);
}

void BotRuntimeAdapterTest::usesCustomCooldownFromStrategy() {
  auto strategy = nenoserpent::adapter::bot::defaultStrategyConfig();
  strategy.choiceCooldownTicks = 9;
  strategy.stateActionCooldownTicks = 7;

  nenoserpent::adapter::bot::RuntimeInput startInput{};
  startInput.enabled = true;
  startInput.state = AppState::StartMenu;
  startInput.strategy = &strategy;
  const auto startResult = nenoserpent::adapter::bot::step(startInput);
  QCOMPARE(startResult.nextCooldownTicks, 7);

  nenoserpent::adapter::bot::RuntimeInput choiceInput{};
  choiceInput.enabled = true;
  choiceInput.state = AppState::ChoiceSelection;
  choiceInput.strategy = &strategy;
  choiceInput.choices = {QVariantMap{{"type", 4}}};
  const auto choiceResult = nenoserpent::adapter::bot::step(choiceInput);
  QCOMPARE(choiceResult.nextCooldownTicks, 9);
}

QTEST_MAIN(BotRuntimeAdapterTest)
#include "test_bot_runtime_adapter.moc"
