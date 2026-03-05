#pragma once

#include <QAbstractListModel>
#include <QColor>
#include <QFile>
#include <QJSEngine>
#include <QObject>
#include <QRandomGenerator>
#include <QRect>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

#include "adapter/bot/port.h"
#include "adapter/haptics/controller.h"
#include "adapter/ui/action.h"
#include "app_state.h"
#include "core/replay/types.h"
#include "core/session/core.h"
#include "core/session/state.h"
#include "game_engine_interface.h"
#include "services/audio/bus.h"
#include "services/level/repository.h"
#include "services/save/repository.h"
#ifdef NENOSERPENT_HAS_SENSORS
#include <QAccelerometer>
#endif
#include <array>
#include <deque>
#include <functional>
#include <memory>
#include <optional>

class ProfileManager;
class GameState;

class SnakeModel final : public QAbstractListModel {
  Q_OBJECT
public:
  enum Roles { PositionRole = Qt::UserRole + 1 };
  explicit SnakeModel(QObject* parent = nullptr)
      : QAbstractListModel(parent) {
  }
  [[nodiscard]] auto rowCount(const QModelIndex& parent = QModelIndex()) const noexcept
    -> int override {
    Q_UNUSED(parent);
    return static_cast<int>(m_body.size());
  }
  [[nodiscard]] auto data(const QModelIndex& index, int role = Qt::DisplayRole) const
    -> QVariant override {
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(m_body.size()))
      return {};
    if (role == PositionRole)
      return m_body[static_cast<size_t>(index.row())];
    return {};
  }
  [[nodiscard]] auto roleNames() const -> QHash<int, QByteArray> override {
    return {{PositionRole, "pos"}};
  }
  [[nodiscard]] auto body() const noexcept -> const std::deque<QPoint>& {
    return m_body;
  }
  void setBodyChangedCallback(std::function<void(const std::deque<QPoint>&)> callback) {
    m_bodyChangedCallback = std::move(callback);
  }
  void reset(const std::deque<QPoint>& newBody) {
    beginResetModel();
    m_body = newBody;
    endResetModel();
    notifyBodyChanged();
  }
  void moveHead(const QPoint& newHead, bool grew) {
    beginInsertRows(QModelIndex(), 0, 0);
    m_body.emplace_front(newHead);
    endInsertRows();
    if (!grew) {
      const int last = static_cast<int>(m_body.size() - 1);
      if (last >= 0) {
        beginRemoveRows(QModelIndex(), last, last);
        m_body.pop_back();
        endRemoveRows();
      }
    }
    notifyBodyChanged();
  }

private:
  void notifyBodyChanged() const {
    if (m_bodyChangedCallback) {
      m_bodyChangedCallback(m_body);
    }
  }

  std::deque<QPoint> m_body;
  std::function<void(const std::deque<QPoint>&)> m_bodyChangedCallback;
};

class EngineAdapter final : public QObject, public IGameEngine {
  Q_OBJECT

public:
  explicit EngineAdapter(QObject* parent = nullptr);
  ~EngineAdapter() override;

  // --- IGameEngine Interface ---
  void setInternalState(int s) override;
  Q_INVOKABLE void requestStateChange(int newState) override;

  // FSM receives read-only model access; structural mutations stay in EngineAdapter.
  [[nodiscard]] auto snakeModel() const -> const SnakeModel* override {
    return &m_snakeModel;
  }
  [[nodiscard]] auto headPosition() const -> QPoint override {
    return m_sessionCore.headPosition();
  }
  void setDirection(const QPoint& direction) override {
    m_sessionCore.setDirection(direction);
  }
  [[nodiscard]] auto currentTick() const -> int override {
    return m_sessionCore.tickCounter();
  }
  // Replay history is captured at tick granularity to keep playback deterministic.
  void recordInputAtCurrentTick(const QPoint& input) override {
    m_currentInputHistory.append(
      {.frame = m_sessionCore.tickCounter(), .dx = input.x(), .dy = input.y()});
  }
  [[nodiscard]] auto foodPos() const -> QPoint override {
    return m_session.food;
  }
  [[nodiscard]] auto currentState() const -> int override {
    return static_cast<int>(m_state);
  }
  [[nodiscard]] auto hasPendingStateChange() const -> bool override {
    return m_pendingStateChange.has_value();
  }
  [[nodiscard]] auto hasSave() const -> bool override;
  [[nodiscard]] auto hasReplay() const noexcept -> bool override;

  auto advanceSessionStep(const nenoserpent::core::SessionAdvanceConfig& config)
    -> nenoserpent::core::SessionAdvanceResult override;

  void restart() override;
  void startReplay() override;
  void loadLastSession() override;
  void togglePause() override;
  Q_INVOKABLE void nextLevel() override;
  Q_INVOKABLE void nextPalette() override;

  void startEngineTimer(int intervalMs = -1) override;
  void stopEngineTimer() override;

  void triggerHaptic(int magnitude) override;
  void emitAudioEvent(nenoserpent::audio::Event event, float pan = 0.0f) override;
  void updatePersistence() override;
  void advancePlayingState() override;
  void enterGameOverState() override;
  void enterReplayState() override;
  void advanceReplayState() override;
  void lazyInit() override;
  void lazyInitState() override;
  void forceUpdate() override {
    update();
  }

  void generateChoices() override;
  Q_INVOKABLE void selectChoice(int index) override;
  [[nodiscard]] auto choiceIndex() const -> int override {
    return m_choiceIndex;
  }
  void setChoiceIndex(int index) override {
    m_choiceIndex = index;
    emit choiceIndexChanged();
  }

  [[nodiscard]] auto libraryIndex() const -> int override {
    return m_libraryIndex;
  }
  [[nodiscard]] auto fruitLibrarySize() const -> int override {
    return fruitLibrary().size();
  }
  Q_INVOKABLE void setLibraryIndex(int index) override {
    if (m_libraryIndex == index)
      return;
    m_libraryIndex = index;
    emit libraryIndexChanged();
  }
  [[nodiscard]] auto medalIndex() const -> int override {
    return m_medalIndex;
  }
  [[nodiscard]] auto medalLibrarySize() const -> int override {
    return medalLibrary().size();
  }
  Q_INVOKABLE void setMedalIndex(int index) override {
    if (m_medalIndex == index)
      return;
    m_medalIndex = index;
    emit medalIndexChanged();
  }

  // --- QML API ---
  Q_INVOKABLE void dispatchUiAction(const QString& action);
  void dispatchUiAction(const nenoserpent::adapter::UiAction& action);
  Q_INVOKABLE void move(int dx, int dy);
  Q_INVOKABLE void startGame() {
    restart();
  }
  Q_INVOKABLE void nextShellColor();
  Q_INVOKABLE void handleBAction();
  Q_INVOKABLE void quitToMenu();
  Q_INVOKABLE void cycleBgm();
  Q_INVOKABLE void toggleMusic();
  Q_INVOKABLE void quit();
  Q_INVOKABLE void handleSelect();
  Q_INVOKABLE void handleStart();
  Q_INVOKABLE void deleteSave();
  Q_INVOKABLE void toggleBotAutoplay();
  Q_INVOKABLE void cycleBotMode();
  Q_INVOKABLE void cycleBotStrategyMode();
  Q_INVOKABLE void resetBotModeDefaults();
  Q_INVOKABLE bool setBotParam(const QString& key, int value);
  [[nodiscard]] auto botStatus() const -> QVariantMap;
  Q_INVOKABLE void debugSeedReplayBuffPreview();
  Q_INVOKABLE void debugSeedChoicePreview(const QVariantList& types = QVariantList());

  // Property Getters
  auto snakeModelPtr() noexcept -> SnakeModel* {
    return &m_snakeModel;
  }
  [[nodiscard]] auto food() const noexcept -> QPoint {
    return m_session.food;
  }
  [[nodiscard]] auto powerUpPos() const noexcept -> QPoint {
    return m_session.powerUpPos;
  }
  [[nodiscard]] auto powerUpType() const noexcept -> int {
    return m_session.powerUpType;
  }
  [[nodiscard]] auto score() const noexcept -> int {
    return m_session.score;
  }
  [[nodiscard]] auto highScore() const -> int;
  [[nodiscard]] auto state() const noexcept -> AppState::Value {
    return m_state;
  }
  [[nodiscard]] auto boardWidth() const noexcept -> int {
    return BOARD_WIDTH;
  }
  [[nodiscard]] auto boardHeight() const noexcept -> int {
    return BOARD_HEIGHT;
  }
  [[nodiscard]] auto palette() const -> QVariantList;
  [[nodiscard]] auto paletteName() const -> QString;
  [[nodiscard]] auto obstacles() const -> QVariantList;
  [[nodiscard]] auto shellColor() const -> QColor;
  [[nodiscard]] auto shellName() const -> QString;
  [[nodiscard]] auto level() const noexcept -> int {
    return m_levelIndex;
  }
  [[nodiscard]] auto currentLevelName() const noexcept -> QString {
    return m_currentLevelName;
  }
  [[nodiscard]] auto ghost() const -> QVariantList;
  [[nodiscard]] auto musicEnabled() const noexcept -> bool;
  [[nodiscard]] auto bgmVariant() const noexcept -> int {
    return m_bgmVariant;
  }
  [[nodiscard]] auto achievements() const -> QVariantList;
  [[nodiscard]] auto medalLibrary() const -> QVariantList;
  [[nodiscard]] auto coverage() const noexcept -> float;
  [[nodiscard]] auto volume() const -> float;
  void setVolume(float v);
  [[nodiscard]] auto reflectionOffset() const noexcept -> QPointF {
    return m_reflectionOffset;
  }
  [[nodiscard]] auto activeBuff() const noexcept -> int {
    return m_session.activeBuff;
  }
  [[nodiscard]] auto buffTicksRemaining() const noexcept -> int {
    return m_session.buffTicksRemaining;
  }
  [[nodiscard]] auto buffTicksTotal() const noexcept -> int {
    return m_session.buffTicksTotal;
  }
  [[nodiscard]] auto shieldActive() const noexcept -> bool {
    return m_session.shieldActive;
  }
  [[nodiscard]] auto choices() const -> QVariantList {
    return m_choices;
  }
  [[nodiscard]] auto choicePending() const noexcept -> bool {
    return m_choicePending;
  }
  [[nodiscard]] auto fruitLibrary() const -> QVariantList;
  [[nodiscard]] auto botAutoplayEnabled() const noexcept -> bool override {
    return m_botControlPort != nullptr ? m_botControlPort->autoplayEnabled() : false;
  }
  [[nodiscard]] auto botModeName() const -> QString {
    return m_botControlPort != nullptr ? m_botControlPort->modeName() : QStringLiteral("off");
  }
  [[nodiscard]] auto botControlPort() const -> nenoserpent::adapter::bot::BotControlPort* {
    return m_botControlPort;
  }

  static constexpr int BOARD_WIDTH = 20;
  static constexpr int BOARD_HEIGHT = 18;

signals:
  void foodChanged();
  void powerUpChanged();
  void buffChanged();
  void scoreChanged();
  void highScoreChanged();
  void stateChanged();
  void requestFeedback(int magnitude);
  void paletteChanged();
  void obstaclesChanged();
  void shellColorChanged();
  void hasSaveChanged();
  void levelChanged();
  void ghostChanged();
  void musicEnabledChanged();
  void achievementsChanged();
  void achievementEarned(QString title);
  void volumeChanged();
  void reflectionOffsetChanged();
  void choicesChanged();
  void choicePendingChanged();
  void choiceIndexChanged();
  void libraryIndexChanged();
  void fruitLibraryChanged();
  void medalIndexChanged();
  void eventPrompt(QString text);
  void botAutoplayChanged();
  void botStrategyChanged();

  void foodEaten(float pan);
  void powerUpEaten();
  void playerCrashed();
  void uiInteractTriggered();
  void audioPlayBeep(int frequencyHz, int durationMs, float pan);
  void audioPlayScoreCue(int cueId, float pan);
  void audioPlayCrash(int durationMs);
  void audioStartMusic(int trackId);
  void audioStopMusic();
  void audioDuckMusic(float scale, int durationMs);
  void audioSetPaused(bool paused);
  void audioSetMusicEnabled(bool enabled);
  void audioSetVolume(float volume);
  void audioSetScore(int score);

private slots:
  void update();

private:
  void setupAudioSignals();
  void setupSensorRuntime();
  void applyReplayTimelineForCurrentTick(int& inputHistoryIndex, int& choiceHistoryIndex);
  void applyPostTickTasks();
  auto driveBotAutoplay() -> bool;
  void updateReflectionFallback();
  [[nodiscard]] auto initialGameplayIntervalMs() const -> int;
  [[nodiscard]] auto gameplayTickIntervalMs() const -> int;
  void dispatchStateCallback(const std::function<void(GameState&)>& callback);
  void applyPendingStateChangeIfNeeded();
  void applyCollisionMitigationEffects(const nenoserpent::core::SessionAdvanceResult& result);
  void applyChoiceTransition();
  void applyFoodConsumptionEffects(float pan, bool triggerChoice, bool spawnPowerUp);
  void applyPowerUpConsumptionEffects(const nenoserpent::core::SessionAdvanceResult& result);
  void applyMovementEffects(const nenoserpent::core::SessionAdvanceResult& result);
  void deactivateBuff();
  void changeState(std::unique_ptr<GameState> newState);
  void spawnFood();
  void spawnPowerUp();
  void syncSnakeModelFromCore();
  void updateHighScore();
  void saveCurrentState();
  void clearSavedState();
  void resetTransientRuntimeState();
  void resetReplayRuntimeTracking();
  void loadLevelData(int index);
  void applyFallbackLevelData(int levelIndex);
  void checkAchievements();
  void runLevelScript();
  void initHumanTeachCapture();
  void recordHumanTeachSample(int dx, int dy);
  void appendHumanTeachCsvRow(const std::array<float, 21>& features, int action);
  static auto isOutOfBounds(const QPoint& p) noexcept -> bool;
  [[nodiscard]] auto saveRepository() const -> nenoserpent::services::SaveRepository;

  SnakeModel m_snakeModel;
  QRandomGenerator m_rng;
  nenoserpent::core::SessionCore m_sessionCore;
  nenoserpent::core::SessionState& m_session;
  AppState::Value m_state = AppState::Splash;
  bool m_choicePending = false;
  int m_choiceIndex = 0;
  int m_libraryIndex = 0;
  int m_medalIndex = 0;
  AppState::Value m_previousAudioState = AppState::Splash;
  int m_noFoodElapsedMs = 0;
  QVariantList m_choices;
  int m_levelIndex = 0;
  QString m_currentLevelName = QStringLiteral("Classic");
  QList<QPoint> m_currentRecording;
  QList<QPoint> m_bestRecording;
  QList<ReplayFrame> m_currentInputHistory;
  QList<ReplayFrame> m_bestInputHistory;
  QList<ChoiceRecord> m_currentChoiceHistory;
  QList<ChoiceRecord> m_bestChoiceHistory;
  bool m_hasAccelerometerReading = false;
  int m_audioStateToken = 0;
  uint m_randomSeed = 0;
  uint m_bestRandomSeed = 0;
  int m_bestLevelIndex = 0;
  int m_ghostFrameIndex = 0;
  int m_replayInputHistoryIndex = 0;
  int m_replayChoiceHistoryIndex = 0;
  qint64 m_sessionStartTime = 0;
  qint64 m_lastUiInteractAudioMs = 0;
  nenoserpent::adapter::haptics::Controller m_haptics;
  QPointF m_reflectionOffset = {0.0, 0.0};
  QJSEngine m_jsEngine;
  QString m_currentScript;
  nenoserpent::services::AudioBus m_audioBus;
  nenoserpent::services::LevelRepository m_levelRepository;

  std::unique_ptr<QTimer> m_timer;
#ifdef NENOSERPENT_HAS_SENSORS
  std::unique_ptr<QAccelerometer> m_accelerometer;
#endif
  std::unique_ptr<ProfileManager> m_profileManager;
  std::deque<QPoint>& m_inputQueue;
  std::unique_ptr<GameState> m_fsmState;
  bool m_musicEnabled = true;
  int m_bgmVariant = 0;
  std::unique_ptr<nenoserpent::adapter::bot::BotControlPort> m_botControlOwner;
  nenoserpent::adapter::bot::BotControlPort* m_botControlPort = nullptr;
  nenoserpent::adapter::bot::BotRuntimePort* m_botRuntimePort = nullptr;
  bool m_stateCallbackInProgress = false;
  std::optional<int> m_pendingStateChange;
  bool m_humanTeachEnabled = false;
  bool m_humanTeachCsvReady = false;
  QString m_humanTeachDatasetPath;

  static constexpr QRect m_boardRect{0, 0, BOARD_WIDTH, BOARD_HEIGHT};
};
