#pragma once

#include <functional>
#include <variant>

#include <QString>

namespace nenoserpent::adapter {

struct UnknownAction {};
struct NavAction {
  int dx = 0;
  int dy = 0;
};
struct PrimaryAction {};
struct StartAction {};
struct SecondaryAction {};
struct SelectShortAction {};
struct BackCommandAction {};
struct ToggleShellColorAction {};
struct ToggleMusicAction {};
struct ToggleBotAction {};
struct ToggleBotStrategyAction {};
struct QuitToMenuAction {};
struct QuitAction {};
struct NextPaletteAction {};
struct DeleteSaveAction {};
struct StateStartMenuAction {};
struct StateSplashAction {};
struct FeedbackLightAction {};
struct FeedbackUiAction {};
struct FeedbackHeavyAction {};
struct SetLibraryIndexAction {
  int value = 0;
};
struct SetMedalIndexAction {
  int value = 0;
};

using UiAction = std::variant<UnknownAction,
                              NavAction,
                              PrimaryAction,
                              StartAction,
                              SecondaryAction,
                              SelectShortAction,
                              BackCommandAction,
                              ToggleShellColorAction,
                              ToggleMusicAction,
                              ToggleBotAction,
                              ToggleBotStrategyAction,
                              QuitToMenuAction,
                              QuitAction,
                              NextPaletteAction,
                              DeleteSaveAction,
                              StateStartMenuAction,
                              StateSplashAction,
                              FeedbackLightAction,
                              FeedbackUiAction,
                              FeedbackHeavyAction,
                              SetLibraryIndexAction,
                              SetMedalIndexAction>;

struct UiActionDispatchCallbacks {
  std::function<void(int, int)> onMove;
  std::function<void()> onStart;
  std::function<void()> onSecondary;
  std::function<void()> onSelect;
  std::function<void()> onBack;
  std::function<void()> onToggleShellColor;
  std::function<void()> onToggleMusic;
  std::function<void()> onToggleBot;
  std::function<void()> onToggleBotStrategy;
  std::function<void()> onQuitToMenu;
  std::function<void()> onQuit;
  std::function<void()> onNextPalette;
  std::function<void()> onDeleteSave;
  std::function<void()> onStateStartMenu;
  std::function<void()> onStateSplash;
  std::function<void()> onFeedbackLight;
  std::function<void()> onFeedbackUi;
  std::function<void()> onFeedbackHeavy;
  std::function<void(int)> onSetLibraryIndex;
  std::function<void(int)> onSetMedalIndex;
};

[[nodiscard]] auto parseUiAction(const QString& action) -> UiAction;
auto dispatchUiAction(const UiAction& action, const UiActionDispatchCallbacks& callbacks) -> void;

} // namespace nenoserpent::adapter
