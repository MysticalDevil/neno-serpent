#pragma once

#include <optional>

#include <QPoint>
#include <QString>
#include <QVariantList>

#include "adapter/bot/config.h"
#include "adapter/bot/controller.h"

namespace nenoserpent::adapter::bot {

class BotBackend {
public:
  virtual ~BotBackend() = default;

  [[nodiscard]] virtual auto name() const -> QString = 0;
  [[nodiscard]] virtual auto isAvailable() const -> bool {
    return true;
  }
  [[nodiscard]] virtual auto decideDirection(const Snapshot& snapshot,
                                             const StrategyConfig& config) const
    -> std::optional<QPoint> = 0;
  [[nodiscard]] virtual auto decideChoice(const QVariantList& choices,
                                          const StrategyConfig& config) const -> int = 0;
  virtual void reset() {
  }
};

[[nodiscard]] auto ruleBackend() -> const BotBackend&;

} // namespace nenoserpent::adapter::bot
