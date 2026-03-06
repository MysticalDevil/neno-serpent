#pragma once

#include <QByteArray>
#include <QHash>
#include <QString>

namespace nenoserpent::adapter::bot {

enum class BotMode {
  Safe,
  Balanced,
  Aggressive,
};

enum class BotBackendMode {
  Off,
  Human,
  Rule,
  Ml,
  MlOnline,
  Search,
};

enum class DecisionPolicy {
  Conservative,
  Balanced,
  Aggressive,
};

struct StrategyConfig {
  struct ModeWeights {
    int openSpaceWeight = 3;
    int safeNeighborWeight = 12;
    int targetDistanceWeight = 8;
    int straightBonus = 4;
    int foodConsumeBonus = 10;
    int trapPenalty = 40;
    int lookaheadDepth = 2;
    int lookaheadWeight = 10;
    int powerTargetPriorityThreshold = 14;
    int powerTargetDistanceSlack = 2;
  };

  struct LoopGuard {
    int loopRepeatPenalty = 56;
    int loopEscapePenalty = 220;
    int repeatStreakPenalty = 20;
    int cycle4Penalty = 240;
    int cycle6Penalty = 280;
    int cycle8Penalty = 340;
    int tabooPenalty = 220;
    int tabooEscapeTicks = 5;
    int tabooNormalTicks = 3;
  };

  struct Recovery {
    int noProgressPenaltyBase = 16;
    int noProgressPenaltyScale = 2;
    int centerBiasWeight = 26;
    int cornerLeaveBonus = 96;
    int cornerStickPenalty = 108;
    int escapeRatioSoftCapPermille = 700;
    int centerRecoverTicks = 36;
  };

  ModeWeights modeWeights{};
  LoopGuard loopGuard{};
  Recovery recovery{};

  int openSpaceWeight = 3;
  int safeNeighborWeight = 12;
  int targetDistanceWeight = 8;
  int straightBonus = 4;
  int foodConsumeBonus = 10;
  int trapPenalty = 40;
  int lookaheadDepth = 2;
  int lookaheadWeight = 10;
  int powerTargetPriorityThreshold = 14;
  int powerTargetDistanceSlack = 2;
  int tieBreakSeed = 0;
  int loopRepeatPenalty = 56;
  int loopEscapePenalty = 220;

  int choiceCooldownTicks = 2;
  int stateActionCooldownTicks = 4;

  QHash<int, int> powerPriorityByType;
};

struct StrategyLoadResult {
  StrategyConfig config;
  bool loaded = false;
  QString profile;
  QString source;
  QString error;
};

[[nodiscard]] auto defaultStrategyConfig() -> const StrategyConfig&;
[[nodiscard]] auto powerPriority(const StrategyConfig& config, int type) -> int;
[[nodiscard]] auto modeName(BotMode mode) -> QString;
[[nodiscard]] auto nextMode(BotMode mode) -> BotMode;
void applyModeDefaults(StrategyConfig& config, BotMode mode);
[[nodiscard]] auto backendModeName(BotBackendMode mode) -> QString;
[[nodiscard]] auto nextBackendMode(BotBackendMode mode) -> BotBackendMode;
[[nodiscard]] auto decisionPolicyName(DecisionPolicy policy) -> QString;
[[nodiscard]] auto parseDecisionPolicy(const QString& raw) -> DecisionPolicy;
[[nodiscard]] auto decisionPolicyFromEnvironment() -> DecisionPolicy;
[[nodiscard]] auto currentBuildProfileName() -> QString;
[[nodiscard]] auto loadStrategyConfigFromJson(const QByteArray& json,
                                              const QString& profile,
                                              const QString& source = QStringLiteral("inline"))
  -> StrategyLoadResult;
[[nodiscard]] auto loadStrategyConfig(
  const QString& profile,
  const QString& overrideFilePath = QString(),
  const QString& resourcePath = QStringLiteral(":/adapter/bot/strategy_profiles.json"))
  -> StrategyLoadResult;

} // namespace nenoserpent::adapter::bot
