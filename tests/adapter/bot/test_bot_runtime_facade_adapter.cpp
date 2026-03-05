#include <QtTest/QtTest>

#include "adapter/bot/runtime_facade.h"

class BotRuntimeFacadeAdapterTest final : public QObject {
  Q_OBJECT

private slots:
  void runTickInvokesCallbacksForStartMenu();
};

void BotRuntimeFacadeAdapterTest::runTickInvokesCallbacksForStartMenu() {
  nenoserpent::adapter::bot::RuntimeFacade facade;
  facade.cycleBackendMode(); // off -> rule

  int choiceSetTo = -1;
  bool startTriggered = false;

  const bool consumeTick = facade.runTick(
    {
      .state = AppState::StartMenu,
      .snapshotInput = {},
      .choices = {},
      .currentChoiceIndex = 0,
    },
    {
      .enqueueDirection = [](const QPoint&) { return false; },
      .setChoiceIndex = [&choiceSetTo](const int index) { choiceSetTo = index; },
      .triggerStart = [&startTriggered]() { startTriggered = true; },
    });

  QVERIFY(consumeTick);
  QVERIFY(startTriggered);
  QCOMPARE(choiceSetTo, -1);
}

QTEST_MAIN(BotRuntimeFacadeAdapterTest)
#include "test_bot_runtime_facade_adapter.moc"
