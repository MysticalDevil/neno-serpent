#include "core/choice/runtime.h"

#include <algorithm>
#include <array>
#include <random>

using namespace Qt::StringLiterals;

namespace nenoserpent::core {
namespace {
constexpr int BuffGhost = 1;
constexpr int BuffSlow = 2;
constexpr int BuffMagnet = 3;
constexpr int BuffShield = 4;
constexpr int BuffPortal = 5;
constexpr int BuffDouble = 6;
constexpr int BuffRich = 7;
constexpr int BuffLaser = 8;
constexpr int BuffMini = 9;
} // namespace

auto pickRoguelikeChoices(const uint seed, const int count) -> QList<ChoiceSpec> {
  if (count <= 0) {
    return {};
  }

  std::array<ChoiceSpec, 9> allChoices{{
    {.type = BuffGhost, .name = u"Ghost"_s, .description = u"Pass through self"_s},
    {.type = BuffSlow, .name = u"Slow"_s, .description = u"Drop speed by 1 tier"_s},
    {.type = BuffMagnet, .name = u"Magnet"_s, .description = u"Attract food"_s},
    {.type = BuffShield, .name = u"Shield"_s, .description = u"One extra life"_s},
    {.type = BuffPortal, .name = u"Portal"_s, .description = u"Phase through walls"_s},
    {.type = BuffDouble, .name = u"Double"_s, .description = u"Double points"_s},
    {.type = BuffRich, .name = u"Diamond"_s, .description = u"Triple points"_s},
    {.type = BuffLaser, .name = u"Laser"_s, .description = u"Break obstacle"_s},
    {.type = BuffMini, .name = u"Mini"_s, .description = u"Shrink body"_s},
  }};

  std::shuffle(allChoices.begin(), allChoices.end(), std::default_random_engine(seed));

  QList<ChoiceSpec> picked;
  const int boundedCount = std::min<int>(count, allChoices.size());
  picked.reserve(boundedCount);
  for (int i = 0; i < boundedCount; ++i) {
    picked.append(allChoices[static_cast<size_t>(i)]);
  }
  return picked;
}

} // namespace nenoserpent::core
