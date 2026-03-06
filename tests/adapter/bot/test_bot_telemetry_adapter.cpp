#include <QtTest/QtTest>

#include "adapter/bot/telemetry.h"

class BotRouteTelemetryAdapterTest final : public QObject {
  Q_OBJECT

private slots:
  void emitsWhenRouteChanges();
  void suppressesWhenRouteUnchanged();
  void tracksDirectionEmptyRuleStatsAndWarnThreshold();
};

void BotRouteTelemetryAdapterTest::emitsWhenRouteChanges() {
  nenoserpent::adapter::bot::State state;
  state.cycleBackendMode(); // off -> human
  state.cycleBackendMode(); // human -> rule

  nenoserpent::adapter::bot::RuntimeOutput decision{};
  decision.backend = QStringLiteral("rule");
  decision.usedFallback = true;
  decision.fallbackReason = QStringLiteral("backend-unavailable");

  const auto telemetry = nenoserpent::adapter::bot::updateRouteTelemetry(state, decision);
  QVERIFY(telemetry.changed);
  QVERIFY(telemetry.usedFallback);
  QCOMPARE(telemetry.backend, QStringLiteral("rule"));
  QCOMPARE(telemetry.fallbackReason, QStringLiteral("backend-unavailable"));
  QCOMPARE(state.lastBackendRoute(), QStringLiteral("rule:backend-unavailable"));
}

void BotRouteTelemetryAdapterTest::suppressesWhenRouteUnchanged() {
  nenoserpent::adapter::bot::State state;
  state.cycleBackendMode(); // off -> human
  state.cycleBackendMode(); // human -> rule
  state.setLastBackendRoute(QStringLiteral("rule"));

  nenoserpent::adapter::bot::RuntimeOutput decision{};
  decision.backend = QStringLiteral("rule");

  const auto telemetry = nenoserpent::adapter::bot::updateRouteTelemetry(state, decision);
  QVERIFY(!telemetry.changed);
}

void BotRouteTelemetryAdapterTest::tracksDirectionEmptyRuleStatsAndWarnThreshold() {
  nenoserpent::adapter::bot::State state;
  bool warned = false;
  for (int i = 0; i < 23; ++i) {
    warned = state.observeDirectionEmptyRuleFallback(true, QStringLiteral("direction-empty-rule"));
    QVERIFY(!warned);
  }
  warned = state.observeDirectionEmptyRuleFallback(true, QStringLiteral("direction-empty-rule"));
  QVERIFY(warned);

  const QVariantMap stats = state.status();
  QCOMPARE(stats.value(QStringLiteral("directionEmptyRuleTotal")).toInt(), 24);
  QCOMPARE(stats.value(QStringLiteral("directionEmptyRuleWindow")).toInt(), 24);
}

QTEST_MAIN(BotRouteTelemetryAdapterTest)
#include "test_bot_telemetry_adapter.moc"
