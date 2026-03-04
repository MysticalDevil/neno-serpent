#include <QtTest/QtTest>

#include "adapter/bot/backend.h"
#include "adapter/bot/runtime.h"

class BotRuntimeAdapterTest final : public QObject {
  Q_OBJECT

private slots:
  void startMenuTriggersStartWhenEnabled();
  void choiceSelectionPicksChoiceAndConfirms();
  void cooldownDelaysNonPlayingActions();
  void usesCustomCooldownFromStrategy();
  void usesInjectedBackendForDirectionAndChoice();
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

void BotRuntimeAdapterTest::usesInjectedBackendForDirectionAndChoice() {
  class FakeBackend final : public nenoserpent::adapter::bot::BotBackend {
  public:
    [[nodiscard]] auto name() const -> QString override {
      return QStringLiteral("fake");
    }
    [[nodiscard]] auto decideDirection(const nenoserpent::adapter::bot::Snapshot&,
                                       const nenoserpent::adapter::bot::StrategyConfig&) const
      -> std::optional<QPoint> override {
      return QPoint(1, 0);
    }
    [[nodiscard]] auto decideChoice(const QVariantList&,
                                    const nenoserpent::adapter::bot::StrategyConfig&) const
      -> int override {
      return 2;
    }
  };

  const FakeBackend backend{};

  nenoserpent::adapter::bot::RuntimeInput playing{};
  playing.enabled = true;
  playing.state = AppState::Playing;
  playing.snapshot.body = {QPoint(10, 10), QPoint(10, 11)};
  playing.backend = &backend;
  const auto playingResult = nenoserpent::adapter::bot::step(playing);
  QVERIFY(playingResult.enqueueDirection.has_value());
  QCOMPARE(*playingResult.enqueueDirection, QPoint(1, 0));

  nenoserpent::adapter::bot::RuntimeInput choice{};
  choice.enabled = true;
  choice.state = AppState::ChoiceSelection;
  choice.choices = {
    QVariantMap{{"type", 1}},
    QVariantMap{{"type", 2}},
    QVariantMap{{"type", 3}},
  };
  choice.backend = &backend;
  const auto choiceResult = nenoserpent::adapter::bot::step(choice);
  QVERIFY(choiceResult.setChoiceIndex.has_value());
  QCOMPARE(*choiceResult.setChoiceIndex, 2);
  QVERIFY(choiceResult.triggerStart);
}

QTEST_MAIN(BotRuntimeAdapterTest)
#include "test_bot_runtime_adapter.moc"
