#include <QtTest/QtTest>

#include "adapter/input/semantics.h"
#include "game_engine_interface.h"

class TestInputSemanticsAdapter : public QObject {
  Q_OBJECT

private slots:
  void testBackActionByState();
};

void TestInputSemanticsAdapter::testBackActionByState() {
  using nenoserpent::adapter::BackAction;

  QCOMPARE(nenoserpent::adapter::resolveBackActionForState(AppState::StartMenu),
           BackAction::QuitApplication);
  QCOMPARE(nenoserpent::adapter::resolveBackActionForState(AppState::Paused), BackAction::QuitToMenu);
  QCOMPARE(nenoserpent::adapter::resolveBackActionForState(AppState::GameOver), BackAction::QuitToMenu);
  QCOMPARE(nenoserpent::adapter::resolveBackActionForState(AppState::Replaying),
           BackAction::QuitToMenu);
  QCOMPARE(nenoserpent::adapter::resolveBackActionForState(AppState::ChoiceSelection),
           BackAction::QuitToMenu);
  QCOMPARE(nenoserpent::adapter::resolveBackActionForState(AppState::Library), BackAction::QuitToMenu);
  QCOMPARE(nenoserpent::adapter::resolveBackActionForState(AppState::MedalRoom),
           BackAction::QuitToMenu);

  QCOMPARE(nenoserpent::adapter::resolveBackActionForState(AppState::Splash), BackAction::None);
  QCOMPARE(nenoserpent::adapter::resolveBackActionForState(AppState::Playing), BackAction::None);
  QCOMPARE(nenoserpent::adapter::resolveBackActionForState(-1), BackAction::None);
}

QTEST_MAIN(TestInputSemanticsAdapter)
#include "test_input_semantics_adapter.moc"
