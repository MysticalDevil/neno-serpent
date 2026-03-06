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
constexpr int BuffGold = 6;
constexpr int BuffLaser = 8;
constexpr int BuffMini = 9;
constexpr int BuffFreeze = 10;
constexpr int BuffScout = 11;
constexpr int BuffVacuum = 12;
constexpr int BuffAnchor = 13;
} // namespace

auto pickRoguelikeChoices(const uint seed, const int count) -> QList<ChoiceSpec> {
  if (count <= 0) {
    return {};
  }

  std::array<ChoiceSpec, 12> allChoices{{
    {.type = BuffGhost, .name = u"Ghost"_s, .description = u"Pass through self"_s},
    {.type = BuffSlow, .name = u"Slow"_s, .description = u"Drop speed by 1 tier"_s},
    {.type = BuffMagnet, .name = u"Magnet"_s, .description = u"Attract food"_s},
    {.type = BuffShield, .name = u"Shield"_s, .description = u"One extra life"_s},
    {.type = BuffPortal, .name = u"Portal"_s, .description = u"Phase through walls"_s},
    {.type = BuffGold, .name = u"Gold"_s, .description = u"Double points"_s},
    {.type = BuffLaser, .name = u"Laser"_s, .description = u"Break obstacle"_s},
    {.type = BuffMini, .name = u"Mini"_s, .description = u"Shrink body"_s},
    {.type = BuffFreeze, .name = u"Freeze"_s, .description = u"Freeze dynamic hazards"_s},
    {.type = BuffScout, .name = u"Scout"_s, .description = u"Reveal safe next cell"_s},
    {.type = BuffVacuum, .name = u"Vacuum"_s, .description = u"Pull nearby targets inward"_s},
    {.type = BuffAnchor, .name = u"Anchor"_s, .description = u"Lock current speed"_s},
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
