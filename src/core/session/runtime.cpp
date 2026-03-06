#include "core/session/runtime.h"

namespace {

auto powerUpSpawnChancePercent() -> int {
#if defined(NENOSERPENT_BUILD_DEBUG)
  return 24;
#elif defined(NENOSERPENT_BUILD_DEV)
  return 18;
#else
  return 10;
#endif
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
auto buildPowerUpResult(const int powerUpType,
                        const int baseDurationTicks,
                        const bool halfDurationForRich)
  -> nenoserpent::core::PowerUpConsumptionResult {
  nenoserpent::core::PowerUpConsumptionResult result;
  const int acquired = powerUpType;
  if (acquired == static_cast<int>(nenoserpent::core::BuffId::Shield)) {
    result.shieldActivated = true;
  }

  if (acquired == static_cast<int>(nenoserpent::core::BuffId::Mini)) {
    result.miniApplied = true;
    result.activeBuffAfter = static_cast<int>(nenoserpent::core::BuffId::None);
  } else if (acquired == static_cast<int>(nenoserpent::core::BuffId::Vacuum)) {
    result.vacuumApplied = true;
    result.activeBuffAfter = static_cast<int>(nenoserpent::core::BuffId::None);
  } else if (acquired == static_cast<int>(nenoserpent::core::BuffId::Slow)) {
    // Slow now permanently reduces speed by one tier, not as a timed active buff.
    result.activeBuffAfter = static_cast<int>(nenoserpent::core::BuffId::None);
  } else {
    result.activeBuffAfter = acquired;
  }

  result.slowMode = (acquired == static_cast<int>(nenoserpent::core::BuffId::Slow));
  if (result.slowMode) {
    result.buffTicksRemaining = 0;
    result.buffTicksTotal = 0;
    return result;
  }

  const nenoserpent::core::BuffId durationBuff =
    halfDurationForRich ? static_cast<nenoserpent::core::BuffId>(result.activeBuffAfter)
                        : static_cast<nenoserpent::core::BuffId>(acquired);
  const int duration = halfDurationForRich
                         ? nenoserpent::core::buffDurationTicks(durationBuff, baseDurationTicks)
                         : baseDurationTicks;
  result.buffTicksRemaining = duration;
  result.buffTicksTotal = duration;
  return result;
}

} // namespace

namespace nenoserpent::core {

// NOLINTBEGIN(bugprone-easily-swappable-parameters)
auto planFoodConsumption(const QPoint& head,
                         const QPoint& food,
                         const int boardWidth,
                         const int boardHeight,
                         const int activeBuff,
                         const bool shieldActive,
                         const int currentScore,
                         const int lastChoiceScore,
                         const QPoint& powerUpPos,
                         const std::function<int(int)>& randomBounded) -> FoodConsumptionResult {
  FoodConsumptionResult result;
  const QPoint wrapped = wrapPoint(head, boardWidth, boardHeight);
  if (wrapped != food) {
    return result;
  }

  result.ate = true;
  result.previousScore = currentScore;
  const int points = foodPointsForBuff(static_cast<BuffId>(activeBuff));
  result.newScore = currentScore + points;
  result.pan = (static_cast<float>(wrapped.x()) / static_cast<float>(boardWidth) - 0.5F) * 1.4F;

  const bool suppressSpecialEvents = activeBuff != static_cast<int>(BuffId::None) || shieldActive;
  if (suppressSpecialEvents) {
    return result;
  }

  const int chancePercent = roguelikeChoiceChancePercent({
    .previousScore = result.previousScore,
    .newScore = result.newScore,
    .lastChoiceScore = lastChoiceScore,
  });
  if (chancePercent >= 100) {
    result.triggerChoice = true;
    return result;
  }
  if (chancePercent > 0 && randomBounded(100) < chancePercent) {
    result.triggerChoice = true;
    return result;
  }

  if (randomBounded(100) < powerUpSpawnChancePercent() && powerUpPos == QPoint(-1, -1)) {
    result.spawnPowerUp = true;
  }
  return result;
}
// NOLINTEND(bugprone-easily-swappable-parameters)

auto planFoodConsumption(const QPoint& head,
                         const SessionState& state,
                         const int boardWidth,
                         const int boardHeight,
                         const std::function<int(int)>& randomBounded) -> FoodConsumptionResult {
  return planFoodConsumption(head,
                             state.food,
                             boardWidth,
                             boardHeight,
                             state.activeBuff,
                             state.shieldActive,
                             state.score,
                             state.lastRoguelikeChoiceScore,
                             state.powerUpPos,
                             randomBounded);
}

auto planPowerUpConsumption(const QPoint& head,
                            const QPoint& powerUpPos,
                            const int powerUpType,
                            const int baseDurationTicks,
                            const bool halfDurationForRich) -> PowerUpConsumptionResult {
  PowerUpConsumptionResult result;
  if (head != powerUpPos) {
    return result;
  }

  result.ate = true;
  result = buildPowerUpResult(powerUpType, baseDurationTicks, halfDurationForRich);
  result.ate = true;
  return result;
}

auto planPowerUpConsumption(const QPoint& head,
                            const SessionState& state,
                            const int baseDurationTicks,
                            const bool halfDurationForRich) -> PowerUpConsumptionResult {
  return planPowerUpConsumption(
    head, state.powerUpPos, state.powerUpType, baseDurationTicks, halfDurationForRich);
}

auto planPowerUpAcquisition(const int powerUpType,
                            const int baseDurationTicks,
                            const bool halfDurationForRich) -> PowerUpConsumptionResult {
  PowerUpConsumptionResult result =
    buildPowerUpResult(powerUpType, baseDurationTicks, halfDurationForRich);
  result.ate = true;
  return result;
}

auto applyMiniShrink(const std::deque<QPoint>& body, const std::size_t minimumLength)
  -> std::deque<QPoint> {
  if (body.size() <= minimumLength) {
    return body;
  }
  std::deque<QPoint> nextBody;
  const std::size_t targetLength = miniShrinkTargetLength(body.size(), minimumLength);
  for (std::size_t i = 0; i < targetLength && i < body.size(); ++i) {
    nextBody.push_back(body[i]);
  }
  return nextBody;
}

auto applyMagnetAttraction(const QPoint& food,
                           const QPoint& head,
                           const int boardWidth,
                           const int boardHeight,
                           const std::function<bool(const QPoint&)>& isOccupied,
                           const QPoint& powerUpPos) -> MagnetAttractionResult {
  MagnetAttractionResult result;
  if (food == head) {
    result.ate = true;
    result.newFood = food;
    return result;
  }

  const QList<QPoint> candidates = magnetCandidateSpots(food, head, boardWidth, boardHeight);
  for (const QPoint& candidate : candidates) {
    if (candidate == food) {
      continue;
    }
    if (candidate == head || (!isOccupied(candidate) && candidate != powerUpPos)) {
      result.moved = true;
      result.ate = (candidate == head);
      result.newFood = candidate;
      return result;
    }
  }
  return result;
}

auto applyMagnetAttraction(const QPoint& head,
                           const int boardWidth,
                           const int boardHeight,
                           const SessionState& state,
                           const std::function<bool(const QPoint&)>& isOccupied)
  -> MagnetAttractionResult {
  return applyMagnetAttraction(
    state.food, head, boardWidth, boardHeight, isOccupied, state.powerUpPos);
}

} // namespace nenoserpent::core
