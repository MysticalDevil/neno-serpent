#pragma once

#include <cstddef>
#include <functional>

namespace nenoserpent::core {

enum class BuffId : int {
  None = 0,
  Ghost = 1,
  Slow = 2,
  Magnet = 3,
  Shield = 4,
  Portal = 5,
  Gold = 6,
  Laser = 7,
  Mini = 8,
  Freeze = 9,
  Scout = 10,
  Vacuum = 11,
  Anchor = 12,
};

auto foodPointsForBuff(BuffId activeBuff) -> int;
auto buffDurationTicks(BuffId acquiredBuff, int baseDurationTicks) -> int;
auto miniShrinkTargetLength(std::size_t currentLength, std::size_t minimumLength = 3)
  -> std::size_t;
auto weightedRandomBuffId(const std::function<int(int)>& pickBounded) -> BuffId;
auto tickBuffCountdown(int& remainingTicks) -> bool;

} // namespace nenoserpent::core
