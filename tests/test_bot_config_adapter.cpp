#include <QtTest/QtTest>

#include "adapter/bot/config.h"

class BotConfigAdapterTest final : public QObject {
  Q_OBJECT

private slots:
  void parsesProfileOverridesFromJson();
  void fallsBackWhenProfileMissing();
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

QTEST_MAIN(BotConfigAdapterTest)
#include "test_bot_config_adapter.moc"
