#include <algorithm>
#include <cstdint>
#include <iostream>
#include <numeric>
#include <vector>

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QVariantList>

#include "adapter/bot/runtime.h"
#include "core/buff/runtime.h"
#include "core/session/runner.h"
#include "services/level/repository.h"

namespace {

auto modeToAppState(const nenoserpent::core::SessionMode mode) -> AppState::Value {
  switch (mode) {
  case nenoserpent::core::SessionMode::Playing:
    return AppState::Playing;
  case nenoserpent::core::SessionMode::ChoiceSelection:
    return AppState::ChoiceSelection;
  case nenoserpent::core::SessionMode::GameOver:
    return AppState::GameOver;
  case nenoserpent::core::SessionMode::Replaying:
    return AppState::Replaying;
  case nenoserpent::core::SessionMode::ReplayFinished:
    return AppState::StartMenu;
  case nenoserpent::core::SessionMode::Idle:
  default:
    return AppState::StartMenu;
  }
}

auto toChoiceModel(const QList<nenoserpent::core::ChoiceSpec>& choices) -> QVariantList {
  QVariantList result;
  result.reserve(choices.size());
  for (const auto& choice : choices) {
    result.append(QVariantMap{
      {QStringLiteral("type"), choice.type},
      {QStringLiteral("name"), choice.name},
      {QStringLiteral("desc"), choice.description},
    });
  }
  return result;
}

struct BenchmarkStats {
  int games = 0;
  int gameOvers = 0;
  int timeouts = 0;
  int maxScore = 0;
  double avgScore = 0.0;
  int medianScore = 0;
  int p95Score = 0;
};

auto runBenchmark(const int games,
                  const int maxTicks,
                  const uint32_t seedBase,
                  const QList<QPoint>& obstacles,
                  const nenoserpent::adapter::bot::StrategyConfig& strategy) -> BenchmarkStats {
  std::vector<int> scores;
  scores.reserve(static_cast<std::size_t>(games));

  int gameOvers = 0;
  int timeouts = 0;

  for (int gameIndex = 0; gameIndex < games; ++gameIndex) {
    const uint32_t gameSeed = seedBase + static_cast<uint32_t>(gameIndex * 37);
    nenoserpent::core::SessionRunner runner;
    runner.startSession(obstacles, gameSeed);

    int cooldown = 0;
    int tickCount = 0;
    int decisions = 0;
    const int maxDecisions = maxTicks * 4;

    while (decisions < maxDecisions) {
      const auto mode = runner.mode();
      if (mode == nenoserpent::core::SessionMode::GameOver) {
        ++gameOvers;
        break;
      }

      if (mode != nenoserpent::core::SessionMode::Playing &&
          mode != nenoserpent::core::SessionMode::ChoiceSelection) {
        break;
      }

      const auto& core = runner.core();
      const auto& state = core.state();
      const auto decision = nenoserpent::adapter::bot::step({
        .enabled = true,
        .cooldownTicks = cooldown,
        .state = modeToAppState(mode),
        .snapshot =
          {
            .head = core.headPosition(),
            .direction = core.direction(),
            .food = state.food,
            .powerUpPos = state.powerUpPos,
            .powerUpType = state.powerUpType,
            .ghostActive = state.activeBuff == static_cast<int>(nenoserpent::core::BuffId::Ghost),
            .shieldActive = state.shieldActive,
            .portalActive = state.activeBuff == static_cast<int>(nenoserpent::core::BuffId::Portal),
            .laserActive = state.activeBuff == static_cast<int>(nenoserpent::core::BuffId::Laser),
            .boardWidth = 20,
            .boardHeight = 18,
            .obstacles = state.obstacles,
            .body = core.body(),
          },
        .choices = toChoiceModel(runner.choices()),
        .strategy = &strategy,
      });
      cooldown = decision.nextCooldownTicks;

      if (mode == nenoserpent::core::SessionMode::ChoiceSelection) {
        if (decision.triggerStart && decision.setChoiceIndex.has_value()) {
          runner.selectChoice(*decision.setChoiceIndex);
        }
        ++decisions;
        continue;
      }

      if (decision.enqueueDirection.has_value()) {
        runner.enqueueDirection(*decision.enqueueDirection);
      }

      const auto tickResult = runner.tick();
      ++tickCount;
      ++decisions;
      if (tickResult.collision) {
        ++gameOvers;
        break;
      }
      if (tickCount >= maxTicks) {
        ++timeouts;
        break;
      }
    }

    scores.push_back(runner.core().state().score);
  }

  std::sort(scores.begin(), scores.end());
  const int maxScore = scores.empty() ? 0 : scores.back();
  const double sum = std::accumulate(scores.begin(), scores.end(), 0.0);
  const double avgScore = scores.empty() ? 0.0 : sum / static_cast<double>(scores.size());
  const int medianScore = scores.empty() ? 0 : scores[scores.size() / 2];
  const std::size_t p95Index = scores.empty() ? 0 : ((scores.size() - 1) * 95) / 100;
  const int p95Score = scores.empty() ? 0 : scores[p95Index];

  return {
    .games = games,
    .gameOvers = gameOvers,
    .timeouts = timeouts,
    .maxScore = maxScore,
    .avgScore = avgScore,
    .medianScore = medianScore,
    .p95Score = p95Score,
  };
}

} // namespace

auto main(int argc, char* argv[]) -> int {
  QCoreApplication app(argc, argv);
  QCommandLineParser parser;
  parser.setApplicationDescription(QStringLiteral("NenoSerpent bot benchmark"));
  parser.addHelpOption();

  QCommandLineOption gamesOption(QStringList{QStringLiteral("g"), QStringLiteral("games")},
                                 QStringLiteral("Number of games to run."),
                                 QStringLiteral("count"),
                                 QStringLiteral("200"));
  QCommandLineOption ticksOption(QStringList{QStringLiteral("t"), QStringLiteral("max-ticks")},
                                 QStringLiteral("Max ticks per game."),
                                 QStringLiteral("count"),
                                 QStringLiteral("4000"));
  QCommandLineOption seedOption(QStringList{QStringLiteral("s"), QStringLiteral("seed")},
                                QStringLiteral("Base random seed."),
                                QStringLiteral("seed"),
                                QStringLiteral("1337"));
  QCommandLineOption levelOption(QStringList{QStringLiteral("l"), QStringLiteral("level")},
                                 QStringLiteral("Level index."),
                                 QStringLiteral("index"),
                                 QStringLiteral("0"));
  QCommandLineOption profileOption(QStringList{QStringLiteral("profile")},
                                   QStringLiteral("Bot strategy profile (debug/dev/release)."),
                                   QStringLiteral("name"),
                                   nenoserpent::adapter::bot::currentBuildProfileName());
  QCommandLineOption strategyFileOption(
    QStringList{QStringLiteral("strategy-file")},
    QStringLiteral("Optional strategy JSON file path override."),
    QStringLiteral("path"));

  parser.addOption(gamesOption);
  parser.addOption(ticksOption);
  parser.addOption(seedOption);
  parser.addOption(levelOption);
  parser.addOption(profileOption);
  parser.addOption(strategyFileOption);
  parser.process(app);

  const int games = std::max(1, parser.value(gamesOption).toInt());
  const int maxTicks = std::max(200, parser.value(ticksOption).toInt());
  const uint32_t seedBase = static_cast<uint32_t>(parser.value(seedOption).toUInt());
  const int levelIndex = std::max(0, parser.value(levelOption).toInt());
  const QString profile = parser.value(profileOption).trimmed().toLower();
  const QString strategyFile = parser.value(strategyFileOption).trimmed();

  const auto strategyLoad = nenoserpent::adapter::bot::loadStrategyConfig(profile, strategyFile);
  if (!strategyLoad.loaded) {
    std::cerr << "[bot-benchmark] strategy fallback to defaults profile="
              << strategyLoad.profile.toStdString()
              << " source=" << strategyLoad.source.toStdString()
              << " reason=" << strategyLoad.error.toStdString() << '\n';
  }

  nenoserpent::services::LevelRepository levels;
  QList<QPoint> obstacles;
  if (const auto level = levels.loadResolvedLevel(levelIndex); level.has_value()) {
    obstacles = level->walls;
  }

  const auto stats = runBenchmark(games, maxTicks, seedBase, obstacles, strategyLoad.config);
  std::cout << "[bot-benchmark] games=" << stats.games << " level=" << levelIndex
            << " profile=" << profile.toStdString() << '\n';
  std::cout << "[bot-benchmark] score.max=" << stats.maxScore << " score.avg=" << stats.avgScore
            << " score.median=" << stats.medianScore << " score.p95=" << stats.p95Score << '\n';
  std::cout << "[bot-benchmark] outcomes.gameOver=" << stats.gameOvers
            << " outcomes.timeout=" << stats.timeouts << '\n';

  return 0;
}
