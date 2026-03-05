#include "adapter/bot/route_telemetry.h"

using namespace Qt::StringLiterals;

namespace nenoserpent::adapter::bot {

auto updateRouteTelemetry(State& state, const RuntimeOutput& decision) -> RouteTelemetry {
  if (!state.autoplayEnabled()) {
    return {};
  }

  const QString routeKey =
    decision.usedFallback ? decision.backend + u":"_s + decision.fallbackReason : decision.backend;
  if (routeKey == state.lastBackendRoute()) {
    return {};
  }
  state.setLastBackendRoute(routeKey);
  return {
    .changed = true,
    .usedFallback = decision.usedFallback,
    .backend = decision.backend,
    .fallbackReason = decision.fallbackReason,
  };
}

} // namespace nenoserpent::adapter::bot
