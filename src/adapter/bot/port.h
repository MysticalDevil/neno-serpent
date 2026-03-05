#pragma once

#include <functional>

#include <QVariantList>
#include <QVariantMap>

#include "adapter/bot/snapshot_builder.h"
#include "app_state.h"

namespace nenoserpent::adapter::bot {

struct RuntimeTickInput {
  AppState::Value state = AppState::Splash;
  SnapshotBuilderInput snapshotInput;
  QVariantList choices;
  int currentChoiceIndex = 0;
};

struct RuntimeTickCallbacks {
  std::function<bool(const QPoint&)> enqueueDirection;
  std::function<void(int)> setChoiceIndex;
  std::function<void()> triggerStart;
};

class BotControlPort {
public:
  virtual ~BotControlPort() = default;
  virtual void initializeFromEnvironment() = 0;
  [[nodiscard]] virtual auto autoplayEnabled() const -> bool = 0;
  [[nodiscard]] virtual auto modeName() const -> QString = 0;
  [[nodiscard]] virtual auto status() const -> QVariantMap = 0;
  virtual void cycleBackendMode() = 0;
  virtual void cycleStrategyMode() = 0;
  virtual void resetStrategyModeDefaults() = 0;
  [[nodiscard]] virtual auto setParam(const QString& key, int value) -> bool = 0;
};

class BotRuntimePort {
public:
  virtual ~BotRuntimePort() = default;
  virtual auto runTick(const RuntimeTickInput& input, const RuntimeTickCallbacks& callbacks)
    -> bool = 0;
};

} // namespace nenoserpent::adapter::bot
