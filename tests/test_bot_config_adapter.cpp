#include <QtTest/QtTest>

#include "adapter/bot/config.h"

class BotConfigAdapterTest final : public QObject {
  Q_OBJECT

private slots:
  void parsesProfileOverridesFromJson();
  void fallsBackWhenProfileMissing();
  void cyclesStrategyModesInExpectedOrder();
  void cyclesBackendModesInExpectedOrder();
};

void BotConfigAdapterTest::parsesProfileOverridesFromJson() {
  const QByteArray json = R"json(
{
  "profiles": {
    "default": {
      "safeNeighborWeight": 10,
      "choiceCooldownTicks": 3
    },
    "debug": {
      "safeNeighborWeight": 16,
      "stateActionCooldownTicks": 9,
      "powerPriorityByType": {
        "4": 99
      }
    }
  }
}
)json";

  const auto result =
    nenoserpent::adapter::bot::loadStrategyConfigFromJson(json, QStringLiteral("debug"));
  QVERIFY(result.loaded);
  QCOMPARE(result.config.safeNeighborWeight, 16);
  QCOMPARE(result.config.choiceCooldownTicks, 3);
  QCOMPARE(result.config.stateActionCooldownTicks, 9);
  QCOMPARE(nenoserpent::adapter::bot::powerPriority(result.config, 4), 99);
}

void BotConfigAdapterTest::fallsBackWhenProfileMissing() {
  const QByteArray json = R"json(
{
  "profiles": {
    "default": {
      "safeNeighborWeight": 10
    }
  }
}
)json";

  const auto result =
    nenoserpent::adapter::bot::loadStrategyConfigFromJson(json, QStringLiteral("release"));
  QVERIFY(!result.loaded);
  QVERIFY(!result.error.isEmpty());
  QCOMPARE(result.config.safeNeighborWeight, 10);
}

void BotConfigAdapterTest::cyclesStrategyModesInExpectedOrder() {
  using nenoserpent::adapter::bot::BotMode;
  QCOMPARE(nenoserpent::adapter::bot::modeName(BotMode::Safe), QStringLiteral("safe"));
  QCOMPARE(nenoserpent::adapter::bot::modeName(BotMode::Balanced), QStringLiteral("balanced"));
  QCOMPARE(nenoserpent::adapter::bot::modeName(BotMode::Aggressive), QStringLiteral("aggressive"));

  QCOMPARE(nenoserpent::adapter::bot::nextMode(BotMode::Safe), BotMode::Balanced);
  QCOMPARE(nenoserpent::adapter::bot::nextMode(BotMode::Balanced), BotMode::Aggressive);
  QCOMPARE(nenoserpent::adapter::bot::nextMode(BotMode::Aggressive), BotMode::Safe);
}

void BotConfigAdapterTest::cyclesBackendModesInExpectedOrder() {
  using nenoserpent::adapter::bot::BotBackendMode;
  QCOMPARE(nenoserpent::adapter::bot::backendModeName(BotBackendMode::Off), QStringLiteral("off"));
  QCOMPARE(nenoserpent::adapter::bot::backendModeName(BotBackendMode::Rule),
           QStringLiteral("rule"));
  QCOMPARE(nenoserpent::adapter::bot::backendModeName(BotBackendMode::Ml), QStringLiteral("ml"));
  QCOMPARE(nenoserpent::adapter::bot::backendModeName(BotBackendMode::Search),
           QStringLiteral("search"));

  QCOMPARE(nenoserpent::adapter::bot::nextBackendMode(BotBackendMode::Off), BotBackendMode::Rule);
  QCOMPARE(nenoserpent::adapter::bot::nextBackendMode(BotBackendMode::Rule), BotBackendMode::Ml);
  QCOMPARE(nenoserpent::adapter::bot::nextBackendMode(BotBackendMode::Ml), BotBackendMode::Search);
  QCOMPARE(nenoserpent::adapter::bot::nextBackendMode(BotBackendMode::Search), BotBackendMode::Off);
}

QTEST_MAIN(BotConfigAdapterTest)
#include "test_bot_config_adapter.moc"
