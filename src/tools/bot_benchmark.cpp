#include <algorithm>
#include <cstdint>
#include <iostream>
#include <numeric>
#include <ranges>
#include <vector>

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QFile>
#include <QTextStream>
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

struct DatasetWriter final {
  explicit DatasetWriter(const QString& path)
      : file(path) {
  }

  QFile file;
  QTextStream stream{&file};
  bool enabled = false;
  bool headerWritten = false;
  int sampleCount = 0;
  int maxSamples = 0;

  auto open(const int maxRows) -> bool {
    maxSamples = maxRows;
    if (file.fileName().trimmed().isEmpty()) {
      return false;
    }
    const bool appendMode = file.exists() && file.size() > 0;
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
      std::cerr << "[bot-benchmark] failed to open dataset file: " << file.fileName().toStdString()
                << '\n';
      return false;
    }
    enabled = true;
    headerWritten = appendMode;
    if (!headerWritten) {
      writeHeader();
    }
    return true;
  }

  [[nodiscard]] auto shouldStop() const -> bool {
    return enabled && maxSamples > 0 && sampleCount >= maxSamples;
  }

  static auto directionClass(const QPoint& direction) -> int {
    if (direction == QPoint(0, -1)) {
      return 0; // up
    }
    if (direction == QPoint(1, 0)) {
      return 1; // right
    }
    if (direction == QPoint(0, 1)) {
      return 2; // down
    }
    if (direction == QPoint(-1, 0)) {
      return 3; // left
    }
    return -1;
  }

  auto writeSample(const nenoserpent::adapter::bot::Snapshot& snapshot,
                   const int levelIndex,
                   const int score,
                   const QPoint& action) -> void {
    if (!enabled || shouldStop()) {
      return;
    }
    const int actionClass = directionClass(action);
    if (actionClass < 0) {
      return;
    }
    auto collides = [&snapshot](const QPoint& cell) -> int {
      const bool outOfBounds = cell.x() < 0 || cell.y() < 0 || cell.x() >= snapshot.boardWidth ||
                               cell.y() >= snapshot.boardHeight;
      if (outOfBounds) {
        return 1;
      }
      if (snapshot.obstacles.contains(cell) ||
          std::ranges::find(snapshot.body, cell) != snapshot.body.end()) {
        return 1;
      }
      return 0;
    };

    const QPoint up(0, -1);
    const QPoint right(1, 0);
    const QPoint down(0, 1);
    const QPoint left(-1, 0);

    stream << levelIndex << ',' << score << ',' << snapshot.body.size() << ',' << snapshot.head.x()
           << ',' << snapshot.head.y() << ',' << snapshot.direction.x() << ','
           << snapshot.direction.y() << ',' << (snapshot.food.x() - snapshot.head.x()) << ','
           << (snapshot.food.y() - snapshot.head.y()) << ','
           << (snapshot.powerUpPos.x() - snapshot.head.x()) << ','
           << (snapshot.powerUpPos.y() - snapshot.head.y()) << ',' << snapshot.powerUpType << ','
           << (snapshot.powerUpType > 0 ? 1 : 0) << ',' << (snapshot.ghostActive ? 1 : 0) << ','
           << (snapshot.shieldActive ? 1 : 0) << ',' << (snapshot.portalActive ? 1 : 0) << ','
           << (snapshot.laserActive ? 1 : 0) << ',' << collides(snapshot.head + up) << ','
           << collides(snapshot.head + right) << ',' << collides(snapshot.head + down) << ','
           << collides(snapshot.head + left) << ',' << actionClass << '\n';
    sampleCount += 1;
  }

  auto close() -> void {
    if (enabled) {
      stream.flush();
      file.close();
    }
  }

private:
  auto writeHeader() -> void {
    stream << "level,score,body_len,head_x,head_y,dir_x,dir_y,food_dx,food_dy,power_dx,power_dy,"
           << "power_type,power_active,ghost_active,shield_active,portal_active,laser_active,"
           << "danger_up,danger_right,danger_down,danger_left,action\n";
    headerWritten = true;
  }
};

auto runBenchmark(const int games,
                  const int maxTicks,
                  const uint32_t seedBase,
                  const QList<QPoint>& obstacles,
                  const nenoserpent::adapter::bot::StrategyConfig& strategy,
                  const int levelIndex,
                  DatasetWriter* datasetWriter) -> BenchmarkStats {
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
      if (datasetWriter != nullptr && datasetWriter->shouldStop()) {
        break;
      }
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
        if (datasetWriter != nullptr) {
          datasetWriter->writeSample(
            {
              .head = core.headPosition(),
              .direction = core.direction(),
              .food = state.food,
              .powerUpPos = state.powerUpPos,
              .powerUpType = state.powerUpType,
              .ghostActive = state.activeBuff == static_cast<int>(nenoserpent::core::BuffId::Ghost),
              .shieldActive = state.shieldActive,
              .portalActive =
                state.activeBuff == static_cast<int>(nenoserpent::core::BuffId::Portal),
              .laserActive = state.activeBuff == static_cast<int>(nenoserpent::core::BuffId::Laser),
              .boardWidth = 20,
              .boardHeight = 18,
              .obstacles = state.obstacles,
              .body = core.body(),
            },
            levelIndex,
            state.score,
            *decision.enqueueDirection);
        }
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

  std::ranges::sort(scores);
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
  QCommandLineOption modeOption(QStringList{QStringLiteral("mode")},
                                QStringLiteral("Bot mode (safe/balanced/aggressive)."),
                                QStringLiteral("name"),
                                QStringLiteral("balanced"));
  QCommandLineOption strategyFileOption(
    QStringList{QStringLiteral("strategy-file")},
    QStringLiteral("Optional strategy JSON file path override."),
    QStringLiteral("path"));
  QCommandLineOption dumpDatasetOption(
    QStringList{QStringLiteral("dump-dataset")},
    QStringLiteral("Optional output CSV path for (state,action) dataset dump."),
    QStringLiteral("path"));
  QCommandLineOption maxSamplesOption(
    QStringList{QStringLiteral("max-samples")},
    QStringLiteral("Maximum dataset rows to dump (0 = unlimited)."),
    QStringLiteral("count"),
    QStringLiteral("0"));

  parser.addOption(gamesOption);
  parser.addOption(ticksOption);
  parser.addOption(seedOption);
  parser.addOption(levelOption);
  parser.addOption(profileOption);
  parser.addOption(modeOption);
  parser.addOption(strategyFileOption);
  parser.addOption(dumpDatasetOption);
  parser.addOption(maxSamplesOption);
  parser.process(app);

  const int games = std::max(1, parser.value(gamesOption).toInt());
  const int maxTicks = std::max(200, parser.value(ticksOption).toInt());
  const uint32_t seedBase = static_cast<uint32_t>(parser.value(seedOption).toUInt());
  const int levelIndex = std::max(0, parser.value(levelOption).toInt());
  const QString profile = parser.value(profileOption).trimmed().toLower();
  const QString mode = parser.value(modeOption).trimmed().toLower();
  const QString strategyFile = parser.value(strategyFileOption).trimmed();
  const QString dumpDatasetPath = parser.value(dumpDatasetOption).trimmed();
  const int maxSamples = std::max(0, parser.value(maxSamplesOption).toInt());

  const auto strategyLoad = nenoserpent::adapter::bot::loadStrategyConfig(profile, strategyFile);
  if (!strategyLoad.loaded) {
    std::cerr << "[bot-benchmark] strategy fallback to defaults profile="
              << strategyLoad.profile.toStdString()
              << " source=" << strategyLoad.source.toStdString()
              << " reason=" << strategyLoad.error.toStdString() << '\n';
  }

  auto strategy = strategyLoad.config;
  if (mode == QStringLiteral("safe")) {
    nenoserpent::adapter::bot::applyModeDefaults(strategy,
                                                 nenoserpent::adapter::bot::BotMode::Safe);
  } else if (mode == QStringLiteral("aggressive")) {
    nenoserpent::adapter::bot::applyModeDefaults(strategy,
                                                 nenoserpent::adapter::bot::BotMode::Aggressive);
  } else {
    nenoserpent::adapter::bot::applyModeDefaults(strategy,
                                                 nenoserpent::adapter::bot::BotMode::Balanced);
  }

  nenoserpent::services::LevelRepository levels;
  QList<QPoint> obstacles;
  if (const auto level = levels.loadResolvedLevel(levelIndex); level.has_value()) {
    obstacles = level->walls;
  }

  DatasetWriter datasetWriter(dumpDatasetPath);
  DatasetWriter* datasetWriterPtr = nullptr;
  if (!dumpDatasetPath.isEmpty() && datasetWriter.open(maxSamples)) {
    datasetWriterPtr = &datasetWriter;
  }

  const auto stats =
    runBenchmark(games, maxTicks, seedBase, obstacles, strategy, levelIndex, datasetWriterPtr);
  datasetWriter.close();
  std::cout << "[bot-benchmark] games=" << stats.games << " level=" << levelIndex
            << " profile=" << profile.toStdString() << " mode=" << mode.toStdString() << '\n';
  std::cout << "[bot-benchmark] score.max=" << stats.maxScore << " score.avg=" << stats.avgScore
            << " score.median=" << stats.medianScore << " score.p95=" << stats.p95Score << '\n';
  std::cout << "[bot-benchmark] outcomes.gameOver=" << stats.gameOvers
            << " outcomes.timeout=" << stats.timeouts << '\n';
  if (datasetWriterPtr != nullptr) {
    std::cout << "[bot-benchmark] dataset.path=" << dumpDatasetPath.toStdString()
              << " dataset.samples=" << datasetWriter.sampleCount << '\n';
  }

  return 0;
}
