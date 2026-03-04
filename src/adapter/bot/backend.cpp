#include "adapter/bot/backend.h"

namespace nenoserpent::adapter::bot {

namespace {

class RuleBackend final : public BotBackend {
public:
  [[nodiscard]] auto name() const -> QString override {
    return QStringLiteral("rule");
  }

  [[nodiscard]] auto decideDirection(const Snapshot& snapshot, const StrategyConfig& config) const
    -> std::optional<QPoint> override {
    return pickDirection(snapshot, config);
  }

  [[nodiscard]] auto decideChoice(const QVariantList& choices, const StrategyConfig& config) const
    -> int override {
    return pickChoiceIndex(choices, config);
  }
};

} // namespace

auto ruleBackend() -> const BotBackend& {
  static const RuleBackend kBackend;
  return kBackend;
}

} // namespace nenoserpent::adapter::bot
