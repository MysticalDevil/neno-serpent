#pragma once

#include "adapter/bot/port.h"
#include "adapter/bot/state.h"

namespace nenoserpent::adapter::bot {

class RuntimeFacade final : public BotControlPort, public BotRuntimePort {
public:
  RuntimeFacade() = default;

  void initializeFromEnvironment() override;
  [[nodiscard]] auto autoplayEnabled() const -> bool override;
  [[nodiscard]] auto modeName() const -> QString override;
  [[nodiscard]] auto status() const -> QVariantMap override;
  void cycleBackendMode() override;
  void cycleStrategyMode() override;
  void resetStrategyModeDefaults() override;
  [[nodiscard]] auto setParam(const QString& key, int value) -> bool override;

  auto runTick(const RuntimeTickInput& input, const RuntimeTickCallbacks& callbacks)
    -> bool override;

private:
  State m_state;
};

} // namespace nenoserpent::adapter::bot
