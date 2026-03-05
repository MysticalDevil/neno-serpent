#include <QtTest/QtTest>

#include "adapter/bot/telemetry.h"

class BotRouteTelemetryAdapterTest final : public QObject {
  Q_OBJECT

private slots:
  void emitsWhenRouteChanges();
  void suppressesWhenRouteUnchanged();
};

void BotRouteTelemetryAdapterTest::emitsWhenRouteChanges() {
  nenoserpent::adapter::bot::State state;
  state.cycleBackendMode(); // off -> rule

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
  state.cycleBackendMode();
  state.setLastBackendRoute(QStringLiteral("rule"));

  nenoserpent::adapter::bot::RuntimeOutput decision{};
  decision.backend = QStringLiteral("rule");

  const auto telemetry = nenoserpent::adapter::bot::updateRouteTelemetry(state, decision);
  QVERIFY(!telemetry.changed);
}

QTEST_MAIN(BotRouteTelemetryAdapterTest)
#include "test_bot_telemetry_adapter.moc"
