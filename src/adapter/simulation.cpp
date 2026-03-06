#include "adapter/engine.h"
#include "adapter/models/library.h"
#include "adapter/profile/bridge.h"
#include "power_up_id.h"

using namespace Qt::StringLiterals;

namespace {

struct SessionStepDriverConfig {
  int activeState = AppState::Playing;
  int collisionTargetState = AppState::GameOver;
  bool consumeInputQueue = true;
  bool recordConsumedInput = true;
  bool emitCrashFeedbackOnCollision = true;
};

auto runSessionStepDriver(IGameEngine& engine, const SessionStepDriverConfig& config) -> bool {
  const auto result = engine.advanceSessionStep({
    .boardWidth = 20,
    .boardHeight = 18,
    .consumeInputQueue = config.consumeInputQueue,
    .pauseOnChoiceTrigger = (config.activeState != AppState::Replaying),
  });

  if (result.consumedInput && config.recordConsumedInput) {
    engine.recordInputAtCurrentTick(result.consumedDirection);
  }

  if (result.collision) {
    if (config.emitCrashFeedbackOnCollision) {
      engine.triggerHaptic(8);
      engine.emitAudioEvent(nenoserpent::audio::Event::Crash);
    }
    engine.requestStateChange(config.collisionTargetState);
    return false;
  }

  return result.appliedMovement && engine.currentState() == config.activeState &&
         !engine.hasPendingStateChange();
}

} // namespace

auto EngineAdapter::advanceSessionStep(const nenoserpent::core::SessionAdvanceConfig& config)
  -> nenoserpent::core::SessionAdvanceResult {
  const auto result = m_sessionCore.advanceSessionStep(
    config, [this](const int bound) { return m_rng.bounded(bound); });

  applyCollisionMitigationEffects(result);

  if (result.ateFood) {
    applyFoodConsumptionEffects(result.foodPan, result.triggerChoice, result.spawnPowerUp);
  }

  if (result.atePowerUp) {
    applyPowerUpConsumptionEffects(result);
  }

  if (result.appliedMovement) {
    applyMovementEffects(result);
  }

  return result;
}

void EngineAdapter::applyReplayTimelineForCurrentTick(int& inputHistoryIndex,
                                                      int& choiceHistoryIndex) {
  const auto result = m_sessionCore.applyReplayTimeline(
    m_bestInputHistory, inputHistoryIndex, m_bestChoiceHistory, choiceHistoryIndex);
  if (result.choiceIndex.has_value()) {
    selectChoice(*result.choiceIndex);
  }
}

void EngineAdapter::advancePlayingState() {
  runSessionStepDriver(*this,
                       {
                         .activeState = AppState::Playing,
                         .collisionTargetState = AppState::GameOver,
                         .consumeInputQueue = true,
                         .recordConsumedInput = true,
                         .emitCrashFeedbackOnCollision = true,
                       });
}

void EngineAdapter::advanceReplayState() {
  applyReplayTimelineForCurrentTick(m_replayInputHistoryIndex, m_replayChoiceHistoryIndex);

  runSessionStepDriver(*this,
                       {
                         .activeState = AppState::Replaying,
                         .collisionTargetState = AppState::StartMenu,
                         .consumeInputQueue = false,
                         .recordConsumedInput = false,
                         .emitCrashFeedbackOnCollision = false,
                       });
}

void EngineAdapter::applyCollisionMitigationEffects(
  const nenoserpent::core::SessionAdvanceResult& result) {
  if (result.consumeLaser && result.obstacleIndex >= 0 &&
      result.obstacleIndex < m_session.obstacles.size()) {
    emit obstaclesChanged();
    triggerHaptic(8);
    emit buffChanged();
  }
  if (result.consumeShield) {
    triggerHaptic(5);
    emit buffChanged();
  }
}

void EngineAdapter::applyChoiceTransition() {
  if (m_state == AppState::Replaying) {
    generateChoices();
  } else {
    requestStateChange(AppState::ChoiceSelection);
  }
}

void EngineAdapter::applyFoodConsumptionEffects(const float pan,
                                                const bool triggerChoice,
                                                const bool spawnPowerUpAfterFood) {
  nenoserpent::adapter::logFoodEaten(m_profileManager.get());
  m_noFoodElapsedMs = 0;
  emit foodEaten(pan);
  m_timer->setInterval(gameplayTickIntervalMs());
  emit scoreChanged();
  spawnFood();

  if (triggerChoice) {
    applyChoiceTransition();
  } else if (spawnPowerUpAfterFood) {
    spawnPowerUp();
  }

  triggerHaptic(std::min(5, 2 + (m_session.score / 10)));
}

void EngineAdapter::applyPowerUpConsumptionEffects(
  const nenoserpent::core::SessionAdvanceResult& result) {
  if (m_state != AppState::Replaying &&
      nenoserpent::adapter::discoverFruit(m_profileManager.get(), m_session.powerUpType)) {
    emit fruitLibraryChanged();
    emit eventPrompt(u"CATALOG UNLOCK: "_s +
                     nenoserpent::adapter::fruitNameForType(m_session.powerUpType));
  }
  if (m_state != AppState::Replaying && m_session.powerUpType == PowerUpId::Ghost) {
    nenoserpent::adapter::logGhostTrigger(m_profileManager.get());
  }
  if (result.miniApplied) {
    syncSnakeModelFromCore();
    emit eventPrompt(u"MINI BLITZ! SIZE CUT"_s);
  }

  emit powerUpEaten();
  m_timer->setInterval(gameplayTickIntervalMs());

  triggerHaptic(5);
  emit buffChanged();
  emit powerUpChanged();
}

void EngineAdapter::applyMovementEffects(const nenoserpent::core::SessionAdvanceResult& result) {
  syncSnakeModelFromCore();
  m_currentRecording.append(m_sessionCore.headPosition());
  m_noFoodElapsedMs += m_timer->interval();

  if (m_ghostFrameIndex < static_cast<int>(m_bestRecording.size())) {
    m_ghostFrameIndex++;
    emit ghostChanged();
  }
  if (result.movedFood) {
    emit foodChanged();
  }
  if (result.magnetAteFood) {
    applyFoodConsumptionEffects(
      result.magnetFoodPan, result.triggerChoiceAfterMagnet, result.spawnPowerUpAfterMagnet);
  }
  checkAchievements();
}

void EngineAdapter::applyPostTickTasks() {
  if (!m_currentScript.isEmpty()) {
    runLevelScript();
  }
  m_sessionCore.finishRuntimeUpdate();
}

void EngineAdapter::deactivateBuff() {
  m_timer->setInterval(gameplayTickIntervalMs());
  emit buffChanged();
}
