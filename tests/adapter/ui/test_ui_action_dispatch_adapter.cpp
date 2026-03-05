#include <QtTest/QtTest>

#include "adapter/ui/action.h"

using nenoserpent::adapter::UiActionDispatchCallbacks;

class UiActionDispatchAdapterTest final : public QObject {
  Q_OBJECT

private slots:
  void dispatchesDirectionalAndIndexActions() {
    int dx = 0;
    int dy = 0;
    int libraryIndex = -1;
    int medalIndex = -1;

    UiActionDispatchCallbacks callbacks;
    callbacks.onMove = [&](int x, int y) {
      dx = x;
      dy = y;
    };
    callbacks.onSetLibraryIndex = [&](int value) { libraryIndex = value; };
    callbacks.onSetMedalIndex = [&](int value) { medalIndex = value; };

    nenoserpent::adapter::dispatchUiAction(nenoserpent::adapter::NavAction{-1, 0}, callbacks);
    QCOMPARE(dx, -1);
    QCOMPARE(dy, 0);

    nenoserpent::adapter::dispatchUiAction(nenoserpent::adapter::SetLibraryIndexAction{7},
                                           callbacks);
    QCOMPARE(libraryIndex, 7);

    nenoserpent::adapter::dispatchUiAction(nenoserpent::adapter::SetMedalIndexAction{3}, callbacks);
    QCOMPARE(medalIndex, 3);
  }

  void dispatchesStartAndBackActions() {
    int startCalls = 0;
    int backCalls = 0;
    int botCalls = 0;
    int strategyCalls = 0;

    UiActionDispatchCallbacks callbacks;
    callbacks.onStart = [&]() { ++startCalls; };
    callbacks.onBack = [&]() { ++backCalls; };
    callbacks.onToggleBot = [&]() { ++botCalls; };
    callbacks.onToggleBotStrategy = [&]() { ++strategyCalls; };

    nenoserpent::adapter::dispatchUiAction(nenoserpent::adapter::PrimaryAction{}, callbacks);
    nenoserpent::adapter::dispatchUiAction(nenoserpent::adapter::StartAction{}, callbacks);
    nenoserpent::adapter::dispatchUiAction(nenoserpent::adapter::BackCommandAction{}, callbacks);
    nenoserpent::adapter::dispatchUiAction(nenoserpent::adapter::ToggleBotAction{}, callbacks);
    nenoserpent::adapter::dispatchUiAction(nenoserpent::adapter::ToggleBotStrategyAction{},
                                           callbacks);

    QCOMPARE(startCalls, 2);
    QCOMPARE(backCalls, 1);
    QCOMPARE(botCalls, 1);
    QCOMPARE(strategyCalls, 1);
  }

  void ignoresUnknownAction() {
    int calls = 0;
    UiActionDispatchCallbacks callbacks;
    callbacks.onStart = [&]() { ++calls; };
    callbacks.onMove = [&](int, int) { ++calls; };
    callbacks.onBack = [&]() { ++calls; };

    nenoserpent::adapter::dispatchUiAction(nenoserpent::adapter::UnknownAction{}, callbacks);
    QCOMPARE(calls, 0);
  }
};

QTEST_MAIN(UiActionDispatchAdapterTest)
#include "test_ui_action_dispatch_adapter.moc"
