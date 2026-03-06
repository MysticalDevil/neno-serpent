#include "core/buff/runtime.h"

#include <algorithm>
#include <array>
#include <utility>

namespace nenoserpent::core {

auto foodPointsForBuff(BuffId activeBuff) -> int {
  if (activeBuff == BuffId::Gold) {
    return 2;
  }
  return 1;
}

auto buffDurationTicks(BuffId acquiredBuff, int baseDurationTicks) -> int {
  if (acquiredBuff == BuffId::Freeze) {
    return std::max(16, (baseDurationTicks * 3) / 5);
  }
  if (acquiredBuff == BuffId::Scout) {
    return std::max(20, (baseDurationTicks * 4) / 5);
  }
  if (acquiredBuff == BuffId::Anchor) {
    return std::max(18, (baseDurationTicks * 2) / 3);
  }
  return baseDurationTicks;
}

auto miniShrinkTargetLength(std::size_t currentLength, std::size_t minimumLength) -> std::size_t {
  return std::max(minimumLength, currentLength / 2);
}

auto weightedRandomBuffId(const std::function<int(int)>& pickBounded) -> BuffId {
  // Keep utility fruits common while gating the stronger tempo/control fruits.
  static constexpr std::array<std::pair<BuffId, int>, 12> weightedTable{{
    {BuffId::Ghost, 3},
    {BuffId::Slow, 3},
    {BuffId::Magnet, 3},
    {BuffId::Shield, 3},
    {BuffId::Portal, 3},
    {BuffId::Gold, 3},
    {BuffId::Laser, 2},
    {BuffId::Mini, 1},
    {BuffId::Freeze, 2},
    {BuffId::Scout, 2},
    {BuffId::Vacuum, 2},
    {BuffId::Anchor, 2},
  }};

  int totalWeight = 0;
  for (const auto& item : weightedTable) {
    totalWeight += item.second;
  }
  int pick = pickBounded(totalWeight);
  for (const auto& item : weightedTable) {
    if (pick < item.second) {
      return item.first;
    }
    pick -= item.second;
  }
  return BuffId::Ghost;
}

auto tickBuffCountdown(int& remainingTicks) -> bool {
  if (remainingTicks <= 0) {
    return false;
  }
  remainingTicks -= 1;
  return remainingTicks <= 0;
}

} // namespace nenoserpent::core
