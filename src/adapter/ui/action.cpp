#include "adapter/ui/action.h"

#include <array>
#include <utility>

#include <QtCore/qstringliteral.h>

using namespace Qt::StringLiterals;

namespace nenoserpent::adapter {

namespace {
template <class... Ts> struct Overloaded : Ts... {
  using Ts::operator()...;
};

template <class... Ts> Overloaded(Ts...) -> Overloaded<Ts...>;

struct StaticActionMapping {
  QStringView key;
  UiAction action;
};

const auto kStaticMappings = std::to_array<StaticActionMapping>({
  {u"nav_up"_s, UiAction{NavAction{0, -1}}},
  {u"nav_down"_s, UiAction{NavAction{0, 1}}},
  {u"nav_left"_s, UiAction{NavAction{-1, 0}}},
  {u"nav_right"_s, UiAction{NavAction{1, 0}}},
  {u"primary"_s, UiAction{PrimaryAction{}}},
  {u"start"_s, UiAction{StartAction{}}},
  {u"secondary"_s, UiAction{SecondaryAction{}}},
  {u"select_short"_s, UiAction{SelectShortAction{}}},
  {u"back"_s, UiAction{BackCommandAction{}}},
  {u"toggle_shell_color"_s, UiAction{ToggleShellColorAction{}}},
  {u"toggle_music"_s, UiAction{ToggleMusicAction{}}},
  {u"toggle_bot"_s, UiAction{ToggleBotAction{}}},
  {u"cycle_bot_mode"_s, UiAction{ToggleBotAction{}}},
  {u"quit_to_menu"_s, UiAction{QuitToMenuAction{}}},
  {u"quit"_s, UiAction{QuitAction{}}},
  {u"next_palette"_s, UiAction{NextPaletteAction{}}},
  {u"delete_save"_s, UiAction{DeleteSaveAction{}}},
  {u"state_start_menu"_s, UiAction{StateStartMenuAction{}}},
  {u"state_splash"_s, UiAction{StateSplashAction{}}},
  {u"feedback_light"_s, UiAction{FeedbackLightAction{}}},
  {u"feedback_ui"_s, UiAction{FeedbackUiAction{}}},
  {u"feedback_heavy"_s, UiAction{FeedbackHeavyAction{}}},
});

auto parseIndexedAction(const QString& action, const QStringView prefix, const auto makeAction)
  -> UiAction {
  if (!action.startsWith(prefix)) {
    return UnknownAction{};
  }

  bool ok = false;
  const int value = action.sliced(prefix.size()).toInt(&ok);
  if (!ok) {
    return UnknownAction{};
  }
  return makeAction(value);
}
} // namespace

auto parseUiAction(const QString& action) -> UiAction {
  for (const auto& mapping : kStaticMappings) {
    if (action == mapping.key) {
      return mapping.action;
    }
  }

  if (const UiAction libraryAction = parseIndexedAction(
        action,
        u"set_library_index:"_s,
        [](const int value) -> UiAction { return SetLibraryIndexAction{value}; });
      !std::holds_alternative<UnknownAction>(libraryAction)) {
    return libraryAction;
  }

  if (const UiAction medalAction =
        parseIndexedAction(action,
                           u"set_medal_index:"_s,
                           [](const int value) -> UiAction { return SetMedalIndexAction{value}; });
      !std::holds_alternative<UnknownAction>(medalAction)) {
    return medalAction;
  }

  return UnknownAction{};
}

auto dispatchUiAction(const UiAction& action, const UiActionDispatchCallbacks& callbacks) -> void {
  std::visit(Overloaded{
               [&](const UnknownAction&) -> void {},
               [&](const NavAction& nav) -> void {
                 if (callbacks.onMove)
                   callbacks.onMove(nav.dx, nav.dy);
               },
               [&](const PrimaryAction&) -> void {
                 if (callbacks.onStart)
                   callbacks.onStart();
               },
               [&](const StartAction&) -> void {
                 if (callbacks.onStart)
                   callbacks.onStart();
               },
               [&](const SecondaryAction&) -> void {
                 if (callbacks.onSecondary)
                   callbacks.onSecondary();
               },
               [&](const SelectShortAction&) -> void {
                 if (callbacks.onSelect)
                   callbacks.onSelect();
               },
               [&](const BackCommandAction&) -> void {
                 if (callbacks.onBack)
                   callbacks.onBack();
               },
               [&](const ToggleShellColorAction&) -> void {
                 if (callbacks.onToggleShellColor)
                   callbacks.onToggleShellColor();
               },
               [&](const ToggleMusicAction&) -> void {
                 if (callbacks.onToggleMusic)
                   callbacks.onToggleMusic();
               },
               [&](const ToggleBotAction&) -> void {
                 if (callbacks.onToggleBot)
                   callbacks.onToggleBot();
               },
               [&](const QuitToMenuAction&) -> void {
                 if (callbacks.onQuitToMenu)
                   callbacks.onQuitToMenu();
               },
               [&](const QuitAction&) -> void {
                 if (callbacks.onQuit)
                   callbacks.onQuit();
               },
               [&](const NextPaletteAction&) -> void {
                 if (callbacks.onNextPalette)
                   callbacks.onNextPalette();
               },
               [&](const DeleteSaveAction&) -> void {
                 if (callbacks.onDeleteSave)
                   callbacks.onDeleteSave();
               },
               [&](const StateStartMenuAction&) -> void {
                 if (callbacks.onStateStartMenu)
                   callbacks.onStateStartMenu();
               },
               [&](const StateSplashAction&) -> void {
                 if (callbacks.onStateSplash)
                   callbacks.onStateSplash();
               },
               [&](const FeedbackLightAction&) -> void {
                 if (callbacks.onFeedbackLight)
                   callbacks.onFeedbackLight();
               },
               [&](const FeedbackUiAction&) -> void {
                 if (callbacks.onFeedbackUi)
                   callbacks.onFeedbackUi();
               },
               [&](const FeedbackHeavyAction&) -> void {
                 if (callbacks.onFeedbackHeavy)
                   callbacks.onFeedbackHeavy();
               },
               [&](const SetLibraryIndexAction& setIndex) -> void {
                 if (callbacks.onSetLibraryIndex)
                   callbacks.onSetLibraryIndex(setIndex.value);
               },
               [&](const SetMedalIndexAction& setIndex) -> void {
                 if (callbacks.onSetMedalIndex)
                   callbacks.onSetMedalIndex(setIndex.value);
               },
             },
             action);
}

} // namespace nenoserpent::adapter
