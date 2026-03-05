#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <limits>
#include <numeric>
#include <ranges>
#include <tuple>
#include <vector>

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QFile>
#include <QTextStream>
#include <QVariantList>

#include "adapter/bot/backend.h"
#include "adapter/bot/features.h"
#include "adapter/bot/ml_backend.h"
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
  int choiceTotal = 0;
  int choiceMatchedOracle = 0;
  double choiceMatchRate = 0.0;
  double choiceAvgPriorityGap = 0.0;
};

struct DatasetContext {
  QString caseId;
  QString backend;
  QString mode;
  int seed = 0;
};

enum class BenchmarkBackend {
  Rule,
  Ml,
  Search,
};

struct DatasetWriter final {
  explicit DatasetWriter(const QString& path, DatasetContext context)
      : file(path),
        context(std::move(context)) {
  }

  QFile file;
  QTextStream stream{&file};
  DatasetContext context;
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

  auto writeSample(const nenoserpent::adapter::bot::Snapshot& snapshot, const QPoint& action)
    -> void {
    if (!enabled || shouldStop()) {
      return;
    }
    const int actionClass = nenoserpent::adapter::bot::directionClass(action);
    if (actionClass < 0) {
      return;
    }
    const auto features = nenoserpent::adapter::bot::extractFeatures(snapshot);
    for (int i = 0; i < nenoserpent::adapter::bot::Features::kSize; ++i) {
      if (i > 0) {
        stream << ',';
      }
      stream << features.values[static_cast<std::size_t>(i)];
    }
    stream << ',' << context.caseId << ',' << context.backend << ',' << context.mode << ','
           << context.seed << ',' << actionClass << '\n';
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
           << "danger_up,danger_right,danger_down,danger_left,"
           << "case_id,backend,mode,seed,action\n";
    headerWritten = true;
  }
};

struct ChoiceDatasetWriter final {
  explicit ChoiceDatasetWriter(const QString& path, DatasetContext context)
      : file(path),
        context(std::move(context)) {
  }

  QFile file;
  QTextStream stream{&file};
  DatasetContext context;
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
      std::cerr << "[bot-benchmark] failed to open choice dataset file: "
                << file.fileName().toStdString() << '\n';
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

  auto writeChoice(const int level,
                   const int score,
                   const QVariantList& choices,
                   const int selectedIndex,
                   const int oracleIndex,
                   const int selectedRank,
                   const int selectedPriority,
                   const int oraclePriority) -> void {
    if (!enabled || shouldStop()) {
      return;
    }
    std::array<int, 3> optionTypes = {-1, -1, -1};
    const int limit = std::min(3, static_cast<int>(choices.size()));
    for (int i = 0; i < limit; ++i) {
      const QVariantMap item = choices[i].toMap();
      optionTypes[static_cast<std::size_t>(i)] = item.value(QStringLiteral("type"), -1).toInt();
    }
    const int selectedType =
      (selectedIndex >= 0 && selectedIndex < choices.size())
        ? choices[selectedIndex].toMap().value(QStringLiteral("type"), -1).toInt()
        : -1;
    const int oracleType =
      (oracleIndex >= 0 && oracleIndex < choices.size())
        ? choices[oracleIndex].toMap().value(QStringLiteral("type"), -1).toInt()
        : -1;

    stream << level << ',' << score << ',' << optionTypes[0] << ',' << optionTypes[1] << ','
           << optionTypes[2] << ',' << selectedIndex << ',' << selectedType << ',' << oracleIndex
           << ',' << oracleType << ',' << selectedRank << ',' << selectedPriority << ','
           << oraclePriority << ',' << context.caseId << ',' << context.backend << ','
           << context.mode << ',' << context.seed << '\n';
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
    stream << "level,score,choice_0_type,choice_1_type,choice_2_type,selected_index,selected_type,"
           << "oracle_index,oracle_type,selected_rank,selected_priority,oracle_priority,"
           << "case_id,backend,mode,seed\n";
    headerWritten = true;
  }
};

struct PowerDatasetWriter final {
  explicit PowerDatasetWriter(const QString& path, DatasetContext context)
      : file(path),
        context(std::move(context)) {
  }

  QFile file;
  QTextStream stream{&file};
  DatasetContext context;
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
      std::cerr << "[bot-benchmark] failed to open power dataset file: "
                << file.fileName().toStdString() << '\n';
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

  auto writeDecision(const nenoserpent::adapter::bot::Snapshot& snapshot,
                     const int chosenActionClass,
                     const int chosenRank,
                     const int oracleActionClass) -> void {
    if (!enabled || shouldStop()) {
      return;
    }
    const auto features = nenoserpent::adapter::bot::extractFeatures(snapshot);
    for (int i = 0; i < nenoserpent::adapter::bot::Features::kSize; ++i) {
      if (i > 0) {
        stream << ',';
      }
      stream << features.values[static_cast<std::size_t>(i)];
    }
    stream << ',' << snapshot.powerUpType << ',' << chosenActionClass << ',' << chosenRank << ','
           << oracleActionClass << ',' << context.caseId << ',' << context.backend << ','
           << context.mode << ',' << context.seed << '\n';
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
           << "danger_up,danger_right,danger_down,danger_left,power_type_runtime,action,"
           << "selected_rank,oracle_action,case_id,backend,mode,seed\n";
    headerWritten = true;
  }
};

constexpr std::array<QPoint, 4> kDirectionClasses = {
  QPoint{0, -1},
  QPoint{1, 0},
  QPoint{0, 1},
  QPoint{-1, 0},
};

auto toroidalDistance(const QPoint& from, const QPoint& to, const int width, const int height)
  -> int {
  const int dx = std::abs(from.x() - to.x());
  const int dy = std::abs(from.y() - to.y());
  return std::min(dx, width - dx) + std::min(dy, height - dy);
}

auto isDirectionAllowed(const nenoserpent::adapter::bot::Snapshot& snapshot,
                        const QPoint& candidate) -> bool {
  if (candidate.x() == -snapshot.direction.x() && candidate.y() == -snapshot.direction.y()) {
    return false;
  }
  const QPoint wrapped = nenoserpent::core::wrapPoint(
    snapshot.head + candidate, snapshot.boardWidth, snapshot.boardHeight);
  if (!snapshot.portalActive && !snapshot.laserActive && snapshot.obstacles.contains(wrapped)) {
    return false;
  }
  if (snapshot.ghostActive) {
    return true;
  }
  const auto bodyIt = std::ranges::find(snapshot.body, wrapped);
  if (bodyIt == snapshot.body.end()) {
    return true;
  }
  if (snapshot.body.empty()) {
    return false;
  }
  const QPoint tail = snapshot.body.back();
  const bool tailWillMove = wrapped != snapshot.food;
  return tailWillMove && wrapped == tail;
}

auto powerActionRank(const nenoserpent::adapter::bot::Snapshot& snapshot, const QPoint& chosen)
  -> std::pair<int, int> {
  const int chosenClass = nenoserpent::adapter::bot::directionClass(chosen);
  if (chosenClass < 0 || snapshot.powerUpPos.x() < 0 || snapshot.powerUpPos.y() < 0) {
    return {-1, -1};
  }
  std::vector<std::tuple<int, int, int>> scored;
  scored.reserve(4);
  for (int cls = 0; cls < 4; ++cls) {
    const QPoint candidate = kDirectionClasses[static_cast<std::size_t>(cls)];
    if (!isDirectionAllowed(snapshot, candidate)) {
      continue;
    }
    const QPoint wrapped = nenoserpent::core::wrapPoint(
      snapshot.head + candidate, snapshot.boardWidth, snapshot.boardHeight);
    const int distance =
      toroidalDistance(wrapped, snapshot.powerUpPos, snapshot.boardWidth, snapshot.boardHeight);
    scored.emplace_back(distance, cls, cls == chosenClass ? 1 : 0);
  }
  if (scored.empty()) {
    return {-1, -1};
  }
  std::ranges::sort(scored, [](const auto& a, const auto& b) {
    if (std::get<0>(a) != std::get<0>(b)) {
      return std::get<0>(a) < std::get<0>(b);
    }
    return std::get<1>(a) < std::get<1>(b);
  });
  int rank = -1;
  int oracleClass = std::get<1>(scored.front());
  for (std::size_t i = 0; i < scored.size(); ++i) {
    if (std::get<2>(scored[i]) == 1) {
      rank = static_cast<int>(i) + 1;
      break;
    }
  }
  return {rank, oracleClass};
}

auto runBenchmark(const int games,
                  const int maxTicks,
                  const uint32_t seedBase,
                  const QList<QPoint>& obstacles,
                  const nenoserpent::adapter::bot::StrategyConfig& strategy,
                  const int levelIndex,
                  const nenoserpent::adapter::bot::BotBackend* primaryBackend,
                  const nenoserpent::adapter::bot::BotBackend* fallbackBackend,
                  DatasetWriter* datasetWriter,
                  ChoiceDatasetWriter* choiceDatasetWriter,
                  PowerDatasetWriter* powerDatasetWriter) -> BenchmarkStats {
  std::vector<int> scores;
  scores.reserve(static_cast<std::size_t>(games));

  int gameOvers = 0;
  int timeouts = 0;
  int choiceTotal = 0;
  int choiceMatchedOracle = 0;
  double choicePriorityGapSum = 0.0;

  for (int gameIndex = 0; gameIndex < games; ++gameIndex) {
    const uint32_t gameSeed = seedBase + static_cast<uint32_t>(gameIndex * 37);
    nenoserpent::core::SessionRunner runner;
    runner.startSession(obstacles, gameSeed);

    int cooldown = 0;
    int tickCount = 0;
    int decisions = 0;
    const int maxDecisions = maxTicks * 4;

    while (decisions < maxDecisions) {
      if ((datasetWriter != nullptr && datasetWriter->shouldStop()) ||
          (choiceDatasetWriter != nullptr && choiceDatasetWriter->shouldStop()) ||
          (powerDatasetWriter != nullptr && powerDatasetWriter->shouldStop())) {
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
            .score = state.score,
            .levelIndex = levelIndex,
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
        .backend = primaryBackend,
        .fallbackBackend = fallbackBackend,
      });
      cooldown = decision.nextCooldownTicks;

      if (mode == nenoserpent::core::SessionMode::ChoiceSelection) {
        const auto choiceModel = toChoiceModel(runner.choices());
        int bestPriority = std::numeric_limits<int>::min();
        int selectedPriority = std::numeric_limits<int>::min();
        if (decision.triggerStart && decision.setChoiceIndex.has_value()) {
          int oracleIndex = -1;
          int oraclePriority = std::numeric_limits<int>::min();
          std::vector<std::pair<int, int>> ranking;
          ranking.reserve(static_cast<std::size_t>(choiceModel.size()));
          for (int i = 0; i < choiceModel.size(); ++i) {
            const QVariantMap item = choiceModel[i].toMap();
            if (item.isEmpty() || !item.contains(QStringLiteral("type"))) {
              continue;
            }
            const int type = item.value(QStringLiteral("type")).toInt();
            const int priority = nenoserpent::adapter::bot::powerPriority(strategy, type);
            bestPriority = std::max(bestPriority, priority);
            ranking.emplace_back(i, priority);
            if (oracleIndex < 0 || priority > oraclePriority ||
                (priority == oraclePriority && i < oracleIndex)) {
              oracleIndex = i;
              oraclePriority = priority;
            }
            if (i == *decision.setChoiceIndex) {
              selectedPriority = priority;
            }
          }
          std::ranges::sort(ranking, [](const auto& a, const auto& b) {
            if (a.second != b.second) {
              return a.second > b.second;
            }
            return a.first < b.first;
          });
          int selectedRank = -1;
          for (std::size_t r = 0; r < ranking.size(); ++r) {
            if (ranking[r].first == *decision.setChoiceIndex) {
              selectedRank = static_cast<int>(r) + 1;
              break;
            }
          }
          if (bestPriority > std::numeric_limits<int>::min() &&
              selectedPriority > std::numeric_limits<int>::min()) {
            ++choiceTotal;
            if (selectedPriority == bestPriority) {
              ++choiceMatchedOracle;
            }
            choicePriorityGapSum += static_cast<double>(bestPriority - selectedPriority);
          }
          if (choiceDatasetWriter != nullptr) {
            choiceDatasetWriter->writeChoice(levelIndex,
                                             state.score,
                                             choiceModel,
                                             *decision.setChoiceIndex,
                                             oracleIndex,
                                             selectedRank,
                                             selectedPriority,
                                             bestPriority);
          }
          runner.selectChoice(*decision.setChoiceIndex);
        }
        ++decisions;
        continue;
      }

      if (decision.enqueueDirection.has_value()) {
        const nenoserpent::adapter::bot::Snapshot snapshot{
          .head = core.headPosition(),
          .direction = core.direction(),
          .food = state.food,
          .powerUpPos = state.powerUpPos,
          .powerUpType = state.powerUpType,
          .score = state.score,
          .levelIndex = levelIndex,
          .ghostActive = state.activeBuff == static_cast<int>(nenoserpent::core::BuffId::Ghost),
          .shieldActive = state.shieldActive,
          .portalActive = state.activeBuff == static_cast<int>(nenoserpent::core::BuffId::Portal),
          .laserActive = state.activeBuff == static_cast<int>(nenoserpent::core::BuffId::Laser),
          .boardWidth = 20,
          .boardHeight = 18,
          .obstacles = state.obstacles,
          .body = core.body(),
        };
        if (datasetWriter != nullptr) {
          datasetWriter->writeSample(snapshot, *decision.enqueueDirection);
        }
        if (powerDatasetWriter != nullptr && snapshot.powerUpPos.x() >= 0 &&
            snapshot.powerUpPos.y() >= 0) {
          const auto [rank, oracleClass] = powerActionRank(snapshot, *decision.enqueueDirection);
          const int chosenClass =
            nenoserpent::adapter::bot::directionClass(*decision.enqueueDirection);
          if (rank > 0 && oracleClass >= 0 && chosenClass >= 0) {
            powerDatasetWriter->writeDecision(snapshot, chosenClass, rank, oracleClass);
          }
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
  const double choiceMatchRate =
    choiceTotal > 0 ? static_cast<double>(choiceMatchedOracle) / static_cast<double>(choiceTotal)
                    : 0.0;
  const double choiceAvgPriorityGap =
    choiceTotal > 0 ? choicePriorityGapSum / static_cast<double>(choiceTotal) : 0.0;

  return {
    .games = games,
    .gameOvers = gameOvers,
    .timeouts = timeouts,
    .maxScore = maxScore,
    .avgScore = avgScore,
    .medianScore = medianScore,
    .p95Score = p95Score,
    .choiceTotal = choiceTotal,
    .choiceMatchedOracle = choiceMatchedOracle,
    .choiceMatchRate = choiceMatchRate,
    .choiceAvgPriorityGap = choiceAvgPriorityGap,
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
  QCommandLineOption backendOption(QStringList{QStringLiteral("backend")},
                                   QStringLiteral("Bot backend (rule/ml/search)."),
                                   QStringLiteral("name"),
                                   QStringLiteral("rule"));
  QCommandLineOption mlModelOption(QStringList{QStringLiteral("ml-model")},
                                   QStringLiteral("Runtime JSON model path for ml backend."),
                                   QStringLiteral("path"));
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
  QCommandLineOption dumpChoiceDatasetOption(
    QStringList{QStringLiteral("dump-choice-dataset")},
    QStringLiteral("Optional output CSV path for choice decision dataset dump."),
    QStringLiteral("path"));
  QCommandLineOption maxChoiceSamplesOption(
    QStringList{QStringLiteral("max-choice-samples")},
    QStringLiteral("Maximum choice dataset rows to dump (0 = unlimited)."),
    QStringLiteral("count"),
    QStringLiteral("0"));
  QCommandLineOption dumpPowerDatasetOption(
    QStringList{QStringLiteral("dump-power-dataset")},
    QStringLiteral("Optional output CSV path for power-up chase decision dataset dump."),
    QStringLiteral("path"));
  QCommandLineOption maxPowerSamplesOption(
    QStringList{QStringLiteral("max-power-samples")},
    QStringLiteral("Maximum power dataset rows to dump (0 = unlimited)."),
    QStringLiteral("count"),
    QStringLiteral("0"));
  QCommandLineOption datasetCaseIdOption(
    QStringList{QStringLiteral("dataset-case-id")},
    QStringLiteral("Optional case id metadata for dataset rows."),
    QStringLiteral("id"));
  QCommandLineOption datasetBackendOption(
    QStringList{QStringLiteral("dataset-backend")},
    QStringLiteral("Optional backend metadata for dataset rows."),
    QStringLiteral("name"),
    QStringLiteral("rule"));
  QCommandLineOption datasetModeOption(QStringList{QStringLiteral("dataset-mode")},
                                       QStringLiteral("Optional mode metadata for dataset rows."),
                                       QStringLiteral("name"),
                                       QStringLiteral("balanced"));
  QCommandLineOption datasetSeedOption(QStringList{QStringLiteral("dataset-seed")},
                                       QStringLiteral("Optional seed metadata for dataset rows."),
                                       QStringLiteral("seed"),
                                       QStringLiteral("0"));

  parser.addOption(gamesOption);
  parser.addOption(ticksOption);
  parser.addOption(seedOption);
  parser.addOption(levelOption);
  parser.addOption(profileOption);
  parser.addOption(modeOption);
  parser.addOption(backendOption);
  parser.addOption(mlModelOption);
  parser.addOption(strategyFileOption);
  parser.addOption(dumpDatasetOption);
  parser.addOption(maxSamplesOption);
  parser.addOption(dumpChoiceDatasetOption);
  parser.addOption(maxChoiceSamplesOption);
  parser.addOption(dumpPowerDatasetOption);
  parser.addOption(maxPowerSamplesOption);
  parser.addOption(datasetCaseIdOption);
  parser.addOption(datasetBackendOption);
  parser.addOption(datasetModeOption);
  parser.addOption(datasetSeedOption);
  parser.process(app);

  const int games = std::max(1, parser.value(gamesOption).toInt());
  const int maxTicks = std::max(200, parser.value(ticksOption).toInt());
  const uint32_t seedBase = static_cast<uint32_t>(parser.value(seedOption).toUInt());
  const int levelIndex = std::max(0, parser.value(levelOption).toInt());
  const QString profile = parser.value(profileOption).trimmed().toLower();
  const QString mode = parser.value(modeOption).trimmed().toLower();
  const QString backendValue = parser.value(backendOption).trimmed().toLower();
  const QString mlModelPath = parser.value(mlModelOption).trimmed();
  const QString strategyFile = parser.value(strategyFileOption).trimmed();
  const QString dumpDatasetPath = parser.value(dumpDatasetOption).trimmed();
  const int maxSamples = std::max(0, parser.value(maxSamplesOption).toInt());
  const QString dumpChoiceDatasetPath = parser.value(dumpChoiceDatasetOption).trimmed();
  const int maxChoiceSamples = std::max(0, parser.value(maxChoiceSamplesOption).toInt());
  const QString dumpPowerDatasetPath = parser.value(dumpPowerDatasetOption).trimmed();
  const int maxPowerSamples = std::max(0, parser.value(maxPowerSamplesOption).toInt());
  const DatasetContext datasetContext = {
    .caseId = parser.value(datasetCaseIdOption).trimmed(),
    .backend = parser.value(datasetBackendOption).trimmed().toLower(),
    .mode = parser.value(datasetModeOption).trimmed().toLower(),
    .seed = parser.value(datasetSeedOption).toInt(),
  };

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

  DatasetWriter datasetWriter(dumpDatasetPath, datasetContext);
  DatasetWriter* datasetWriterPtr = nullptr;
  if (!dumpDatasetPath.isEmpty() && datasetWriter.open(maxSamples)) {
    datasetWriterPtr = &datasetWriter;
  }
  ChoiceDatasetWriter choiceDatasetWriter(dumpChoiceDatasetPath, datasetContext);
  ChoiceDatasetWriter* choiceDatasetWriterPtr = nullptr;
  if (!dumpChoiceDatasetPath.isEmpty() && choiceDatasetWriter.open(maxChoiceSamples)) {
    choiceDatasetWriterPtr = &choiceDatasetWriter;
  }
  PowerDatasetWriter powerDatasetWriter(dumpPowerDatasetPath, datasetContext);
  PowerDatasetWriter* powerDatasetWriterPtr = nullptr;
  if (!dumpPowerDatasetPath.isEmpty() && powerDatasetWriter.open(maxPowerSamples)) {
    powerDatasetWriterPtr = &powerDatasetWriter;
  }

  BenchmarkBackend backend = BenchmarkBackend::Rule;
  if (backendValue == QStringLiteral("ml")) {
    backend = BenchmarkBackend::Ml;
  } else if (backendValue == QStringLiteral("search")) {
    backend = BenchmarkBackend::Search;
  }

  const nenoserpent::adapter::bot::BotBackend* primaryBackend =
    &nenoserpent::adapter::bot::ruleBackend();
  const nenoserpent::adapter::bot::BotBackend* fallbackBackend =
    &nenoserpent::adapter::bot::ruleBackend();
  nenoserpent::adapter::bot::MlBackend mlBackend;
  if (backend == BenchmarkBackend::Ml) {
    const auto parseFloatOrDefault = [](const QString& text, const float fallback) -> float {
      if (text.trimmed().isEmpty()) {
        return fallback;
      }
      bool ok = false;
      const float value = text.toFloat(&ok);
      return ok ? value : fallback;
    };
    const float minConfidence =
      parseFloatOrDefault(qEnvironmentVariable("NENOSERPENT_BOT_ML_MIN_CONF"), 0.90F);
    const float minMargin =
      parseFloatOrDefault(qEnvironmentVariable("NENOSERPENT_BOT_ML_MIN_MARGIN"), 1.20F);
    mlBackend.setConfidenceGate(minConfidence, minMargin);
    if (mlModelPath.isEmpty()) {
      std::cerr << "[bot-benchmark] ml backend requested without --ml-model, fallback to rule\n";
    } else if (mlBackend.loadFromFile(mlModelPath)) {
      primaryBackend = &mlBackend;
      std::cout << "[bot-benchmark] ml-gate.conf=" << minConfidence
                << " ml-gate.margin=" << minMargin << '\n';
    } else {
      std::cerr << "[bot-benchmark] ml model unavailable path=" << mlModelPath.toStdString()
                << " reason=" << mlBackend.errorString().toStdString() << " fallback=rule\n";
    }
  }
  if (backend == BenchmarkBackend::Search) {
    primaryBackend = &nenoserpent::adapter::bot::searchBackend();
  }

  const auto stats = runBenchmark(games,
                                  maxTicks,
                                  seedBase,
                                  obstacles,
                                  strategy,
                                  levelIndex,
                                  primaryBackend,
                                  fallbackBackend,
                                  datasetWriterPtr,
                                  choiceDatasetWriterPtr,
                                  powerDatasetWriterPtr);
  datasetWriter.close();
  choiceDatasetWriter.close();
  powerDatasetWriter.close();
  std::cout << "[bot-benchmark] games=" << stats.games << " level=" << levelIndex
            << " profile=" << profile.toStdString() << " mode=" << mode.toStdString()
            << " backend=" << backendValue.toStdString() << '\n';
  std::cout << "[bot-benchmark] score.max=" << stats.maxScore << " score.avg=" << stats.avgScore
            << " score.median=" << stats.medianScore << " score.p95=" << stats.p95Score << '\n';
  std::cout << "[bot-benchmark] outcomes.gameOver=" << stats.gameOvers
            << " outcomes.timeout=" << stats.timeouts << '\n';
  std::cout << "[bot-benchmark] choice.total=" << stats.choiceTotal
            << " choice.match=" << stats.choiceMatchedOracle
            << " choice.match_rate=" << stats.choiceMatchRate
            << " choice.avg_gap=" << stats.choiceAvgPriorityGap << '\n';
  if (datasetWriterPtr != nullptr) {
    std::cout << "[bot-benchmark] dataset.path=" << dumpDatasetPath.toStdString()
              << " dataset.samples=" << datasetWriter.sampleCount << '\n';
  }
  if (choiceDatasetWriterPtr != nullptr) {
    std::cout << "[bot-benchmark] choice_dataset.path=" << dumpChoiceDatasetPath.toStdString()
              << " choice_dataset.samples=" << choiceDatasetWriter.sampleCount << '\n';
  }
  if (powerDatasetWriterPtr != nullptr) {
    std::cout << "[bot-benchmark] power_dataset.path=" << dumpPowerDatasetPath.toStdString()
              << " power_dataset.samples=" << powerDatasetWriter.sampleCount << '\n';
  }

  return 0;
}
