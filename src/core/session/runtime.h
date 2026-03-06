#pragma once

#include <deque>
#include <functional>

#include <QList>
#include <QPoint>

#include "core/buff/runtime.h"
#include "core/game/rules.h"
#include "core/session/state.h"

namespace nenoserpent::core {

struct FoodConsumptionResult {
  bool ate = false;
  int previousScore = 0;
  int newScore = 0;
  bool triggerChoice = false;
  bool spawnPowerUp = false;
  float pan = 0.0F;
};

struct PowerUpConsumptionResult {
  bool ate = false;
  int activeBuffAfter = 0;
  bool shieldActivated = false;
  bool miniApplied = false;
  bool vacuumApplied = false;
  int buffTicksRemaining = 0;
  int buffTicksTotal = 0;
  bool slowMode = false;
};

struct MagnetAttractionResult {
  bool moved = false;
  bool ate = false;
  QPoint newFood;
};

auto planFoodConsumption(const QPoint& head,
                         const QPoint& food,
                         int boardWidth,
                         int boardHeight,
                         int activeBuff,
                         bool shieldActive,
                         int currentScore,
                         int lastChoiceScore,
                         const QPoint& powerUpPos,
                         const std::function<int(int)>& randomBounded) -> FoodConsumptionResult;

auto planFoodConsumption(const QPoint& head,
                         const SessionState& state,
                         int boardWidth,
                         int boardHeight,
                         const std::function<int(int)>& randomBounded) -> FoodConsumptionResult;

auto planPowerUpConsumption(const QPoint& head,
                            const QPoint& powerUpPos,
                            int powerUpType,
                            int baseDurationTicks,
                            bool halfDurationForRich) -> PowerUpConsumptionResult;

auto planPowerUpConsumption(const QPoint& head,
                            const SessionState& state,
                            int baseDurationTicks,
                            bool halfDurationForRich) -> PowerUpConsumptionResult;

auto planPowerUpAcquisition(int powerUpType, int baseDurationTicks, bool halfDurationForRich)
  -> PowerUpConsumptionResult;

auto applyMiniShrink(const std::deque<QPoint>& body, std::size_t minimumLength = 3)
  -> std::deque<QPoint>;

auto applyMagnetAttraction(const QPoint& food,
                           const QPoint& head,
                           int boardWidth,
                           int boardHeight,
                           const std::function<bool(const QPoint&)>& isOccupied,
                           const QPoint& powerUpPos) -> MagnetAttractionResult;

auto applyMagnetAttraction(const QPoint& head,
                           int boardWidth,
                           int boardHeight,
                           const SessionState& state,
                           const std::function<bool(const QPoint&)>& isOccupied)
  -> MagnetAttractionResult;

} // namespace nenoserpent::core
