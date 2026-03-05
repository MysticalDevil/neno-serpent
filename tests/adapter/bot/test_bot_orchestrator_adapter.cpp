#include <QtTest/QtTest>

#include "adapter/bot/orchestrator.h"

class BotOrchestratorAdapterTest final : public QObject {
  Q_OBJECT

private slots:
  void updatesCooldownAndRouteWhenAutoplayEnabled();
  void keepsRouteStableAcrossIdenticalTicks();
};

void BotOrchestratorAdapterTest::updatesCooldownAndRouteWhenAutoplayEnabled() {
  nenoserpent::adapter::bot::State botState;
  botState.cycleBackendMode(); // off -> rule

  const nenoserpent::adapter::bot::OrchestratorInput input{
    .state = AppState::StartMenu,
    .snapshot = {},
    .choices = {},
    .currentChoiceIndex = 0,
  };

  const auto output = nenoserpent::adapter::bot::runOrchestratorTick(botState, input);
  QVERIFY(output.routeTelemetry.changed);
  QCOMPARE(output.routeTelemetry.backend, QStringLiteral("rule"));
  QVERIFY(!output.routeTelemetry.usedFallback);
  QVERIFY(output.decision.triggerStart);
  QCOMPARE(botState.lastBackendRoute(), QStringLiteral("rule"));
  QCOMPARE(botState.actionCooldownTicks(), botState.strategyConfig().stateActionCooldownTicks);
}

void BotOrchestratorAdapterTest::keepsRouteStableAcrossIdenticalTicks() {
  nenoserpent::adapter::bot::State botState;
  botState.cycleBackendMode(); // off -> rule

  const nenoserpent::adapter::bot::OrchestratorInput input{
    .state = AppState::StartMenu,
    .snapshot = {},
    .choices = {},
    .currentChoiceIndex = 0,
  };

  const auto first = nenoserpent::adapter::bot::runOrchestratorTick(botState, input);
  QVERIFY(first.routeTelemetry.changed);

  botState.setActionCooldownTicks(0);
  const auto second = nenoserpent::adapter::bot::runOrchestratorTick(botState, input);
  QVERIFY(!second.routeTelemetry.changed);
  QCOMPARE(botState.lastBackendRoute(), QStringLiteral("rule"));
}

QTEST_MAIN(BotOrchestratorAdapterTest)
#include "test_bot_orchestrator_adapter.moc"
