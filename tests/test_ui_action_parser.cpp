#include <QtTest/QtTest>

#include "adapter/ui/action.h"

class TestUiActionParser : public QObject {
  Q_OBJECT

private slots:
  void testKnownActionsMapToExpectedKinds();
  void testIndexedActionsParsePayload();
  void testUnknownAndInvalidActionsFallbackToUnknown();
};

void TestUiActionParser::testKnownActionsMapToExpectedKinds() {
  using nenoserpent::adapter::FeedbackUiAction;
  using nenoserpent::adapter::NavAction;
  using nenoserpent::adapter::PrimaryAction;
  using nenoserpent::adapter::StartAction;
  using nenoserpent::adapter::ToggleBotAction;
  using nenoserpent::adapter::ToggleBotStrategyAction;
  using nenoserpent::adapter::ToggleMusicAction;

  QCOMPARE(std::get<NavAction>(nenoserpent::adapter::parseUiAction("nav_up")).dy, -1);
  QVERIFY(std::holds_alternative<PrimaryAction>(nenoserpent::adapter::parseUiAction("primary")));
  QVERIFY(std::holds_alternative<StartAction>(nenoserpent::adapter::parseUiAction("start")));
  QVERIFY(
    std::holds_alternative<ToggleMusicAction>(nenoserpent::adapter::parseUiAction("toggle_music")));
  QVERIFY(
    std::holds_alternative<ToggleBotAction>(nenoserpent::adapter::parseUiAction("toggle_bot")));
  QVERIFY(std::holds_alternative<ToggleBotStrategyAction>(
    nenoserpent::adapter::parseUiAction("cycle_bot_strategy")));
  QVERIFY(
    std::holds_alternative<FeedbackUiAction>(nenoserpent::adapter::parseUiAction("feedback_ui")));
}

void TestUiActionParser::testIndexedActionsParsePayload() {
  using nenoserpent::adapter::SetLibraryIndexAction;
  using nenoserpent::adapter::SetMedalIndexAction;

  const auto libraryAction = nenoserpent::adapter::parseUiAction("set_library_index:7");
  QCOMPARE(std::get<SetLibraryIndexAction>(libraryAction).value, 7);

  const auto medalAction = nenoserpent::adapter::parseUiAction("set_medal_index:3");
  QCOMPARE(std::get<SetMedalIndexAction>(medalAction).value, 3);
}

void TestUiActionParser::testUnknownAndInvalidActionsFallbackToUnknown() {
  using nenoserpent::adapter::UnknownAction;

  QVERIFY(std::holds_alternative<UnknownAction>(
    nenoserpent::adapter::parseUiAction("not_a_real_action")));
  QVERIFY(std::holds_alternative<UnknownAction>(
    nenoserpent::adapter::parseUiAction("set_library_index:abc")));
  QVERIFY(std::holds_alternative<UnknownAction>(
    nenoserpent::adapter::parseUiAction("set_medal_index:bad")));
}

QTEST_MAIN(TestUiActionParser)
#include "test_ui_action_parser.moc"
