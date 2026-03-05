#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <unordered_map>

#include <QList>
#include <QPoint>

#include "core/game/rules.h"
#include "core/replay/types.h"
#include "core/session/runtime.h"
#include "core/session/snapshot.h"
#include "core/session/step_types.h"

namespace nenoserpent::core {

struct PreviewSeed {
  QList<QPoint> obstacles;
  std::deque<QPoint> body;
  QPoint food = {0, 0};
  QPoint direction = {0, -1};
  QPoint powerUpPos = {-1, -1};
  int powerUpType = 0;
  int powerUpTicksRemaining = 0;
  int score = 0;
  int tickCounter = 0;
  int activeBuff = 0;
  int buffTicksRemaining = 0;
  int buffTicksTotal = 0;
  bool shieldActive = false;
};

struct ReplayTimelineApplication {
  bool appliedInput = false;
  QPoint appliedDirection = {0, 0};
  std::optional<int> choiceIndex;
};

struct RuntimeUpdateResult {
  bool buffExpired = false;
  bool powerUpExpired = false;
};

struct TickCommand {
  SessionAdvanceConfig advanceConfig;
  const QList<ReplayFrame>* replayInputFrames = nullptr;
  int* replayInputHistoryIndex = nullptr;
  const QList<ChoiceRecord>* replayChoiceFrames = nullptr;
  int* replayChoiceHistoryIndex = nullptr;
  bool applyRuntimeHooks = true;
};

struct TickResult {
  RuntimeUpdateResult runtimeUpdate;
  ReplayTimelineApplication replayTimeline;
  SessionAdvanceResult step;
};

struct MetaAction {
  enum class Kind {
    ResetTransientRuntime,
    ResetReplayRuntime,
    BootstrapForLevel,
    RestorePersistedSession,
    SeedPreviewState,
    RestoreSnapshot,
  };

  Kind kind = Kind::ResetTransientRuntime;
  QList<QPoint> obstacles;
  StateSnapshot snapshot;
  PreviewSeed previewSeed;
  int boardWidth = 0;
  int boardHeight = 0;

  [[nodiscard]] static auto resetTransientRuntime() -> MetaAction;
  [[nodiscard]] static auto resetReplayRuntime() -> MetaAction;
  [[nodiscard]] static auto
  bootstrapForLevel(QList<QPoint> obstacles, int boardWidth, int boardHeight) -> MetaAction;
  [[nodiscard]] static auto restorePersistedSession(const StateSnapshot& snapshot) -> MetaAction;
  [[nodiscard]] static auto seedPreviewState(const PreviewSeed& seed) -> MetaAction;
  [[nodiscard]] static auto restoreSnapshot(const StateSnapshot& snapshot) -> MetaAction;
};

class SessionCore {
public:
  [[nodiscard]] auto state() -> SessionState& {
    return m_state;
  }
  [[nodiscard]] auto state() const -> const SessionState& {
    return m_state;
  }
  [[nodiscard]] auto body() -> std::deque<QPoint>& {
    return m_body;
  }
  [[nodiscard]] auto body() const -> const std::deque<QPoint>& {
    return m_body;
  }

  [[nodiscard]] auto inputQueue() -> std::deque<QPoint>& {
    return m_inputQueue;
  }
  [[nodiscard]] auto inputQueue() const -> const std::deque<QPoint>& {
    return m_inputQueue;
  }

  void setDirection(const QPoint& direction);
  [[nodiscard]] auto direction() const -> QPoint;

  [[nodiscard]] auto tickCounter() const -> int;
  [[nodiscard]] auto currentTickIntervalMs() const -> int;
  [[nodiscard]] auto headPosition() const -> QPoint;

  auto enqueueDirection(const QPoint& direction, std::size_t maxQueueSize = 2) -> bool;
  auto consumeQueuedInput(QPoint& nextInput) -> bool;
  void clearQueuedInput();
  void setBody(const std::deque<QPoint>& body);
  void applyMovement(const QPoint& newHead, bool grew);
  auto checkCollision(const QPoint& head, int boardWidth, int boardHeight) -> CollisionOutcome;
  auto consumeFood(const QPoint& head,
                   int boardWidth,
                   int boardHeight,
                   const std::function<int(int)>& randomBounded) -> FoodConsumptionResult;
  auto consumePowerUp(const QPoint& head, int baseDurationTicks, bool halfDurationForRich)
    -> PowerUpConsumptionResult;
  auto applyChoiceSelection(int powerUpType, int baseDurationTicks, bool halfDurationForRich)
    -> PowerUpConsumptionResult;
  auto selectChoice(int powerUpType, int baseDurationTicks, bool halfDurationForRich)
    -> PowerUpConsumptionResult;
  auto spawnFood(int boardWidth, int boardHeight, const std::function<int(int)>& randomBounded)
    -> bool;
  auto spawnPowerUp(int boardWidth, int boardHeight, const std::function<int(int)>& randomBounded)
    -> bool;
  auto applyMagnetAttraction(int boardWidth, int boardHeight) -> MagnetAttractionResult;
  auto applyReplayTimeline(const QList<ReplayFrame>& inputFrames,
                           int& inputHistoryIndex,
                           const QList<ChoiceRecord>& choiceFrames,
                           int& choiceHistoryIndex) -> ReplayTimelineApplication;
  auto beginRuntimeUpdate() -> RuntimeUpdateResult;
  void finishRuntimeUpdate();
  auto tick(const TickCommand& command, const std::function<int(int)>& randomBounded) -> TickResult;
  auto advanceSessionStep(const SessionAdvanceConfig& config,
                          const std::function<int(int)>& randomBounded) -> SessionAdvanceResult;
  void applyMetaAction(const MetaAction& action);
  void bootstrapForLevel(QList<QPoint> obstacles, int boardWidth, int boardHeight);
  void restorePersistedSession(const StateSnapshot& snapshot);
  void seedPreviewState(const PreviewSeed& seed);

  void resetTransientRuntimeState();
  void resetReplayRuntimeState();

  [[nodiscard]] auto snapshot(const std::deque<QPoint>& body) const -> StateSnapshot;
  void restoreSnapshot(const StateSnapshot& snapshot);

private:
  void incrementTick();
  auto tickBuffCountdown() -> bool;
  auto tickPowerUpCountdown() -> bool;
  [[nodiscard]] auto isOccupied(const QPoint& point) const -> bool;
  void applyPowerUpResult(const PowerUpConsumptionResult& result);
  void resetStallGuard();
  [[nodiscard]] auto stallStateHash() const -> std::uint64_t;
  auto observeStallStateAndMaybeResetTarget(const SessionAdvanceConfig& config,
                                            const std::function<int(int)>& randomBounded,
                                            SessionAdvanceResult& result) -> bool;

  SessionState m_state;
  std::deque<QPoint> m_body;
  std::deque<QPoint> m_inputQueue;
  int m_stallNoScoreTicks = 0;
  int m_stallLastScore = 0;
  std::deque<std::uint64_t> m_stallRecentHashes;
  std::unordered_map<std::uint64_t, int> m_stallHashCounts;
  std::uint64_t m_lastObstacleSignature = 0;
  bool m_hasLastObstacleSignature = false;
  int m_dynamicObstacleConfidenceTicks = 0;
  QList<QPoint> m_prevObstacleSnapshot;
  QList<QPoint> m_currObstacleSnapshot;
  bool m_hasObstacleSnapshots = false;
  std::deque<QPoint> m_recentSpawnPoints;
};

} // namespace nenoserpent::core
