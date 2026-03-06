#include "adapter/bot/ml_backend.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <deque>
#include <cmath>
#include <limits>
#include <ranges>
#include <vector>

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "adapter/bot/controller.h"
#include "adapter/bot/features.h"
#include "core/game/rules.h"

namespace nenoserpent::adapter::bot {

namespace {

constexpr std::array<QPoint, 4> kDirections = {
  QPoint{0, -1},
  QPoint{1, 0},
  QPoint{0, 1},
  QPoint{-1, 0},
};

auto parseFloatArray(const QJsonValue& value, const int expectedSize, std::vector<float>& out)
  -> bool {
  if (!value.isArray()) {
    return false;
  }
  const auto array = value.toArray();
  if (array.size() != expectedSize) {
    return false;
  }
  out.clear();
  out.reserve(static_cast<std::size_t>(expectedSize));
  for (const auto item : array) {
    if (!item.isDouble()) {
      return false;
    }
    out.push_back(static_cast<float>(item.toDouble()));
  }
  return true;
}

auto isReverseDirection(const QPoint& a, const QPoint& b) -> bool {
  return a.x() == -b.x() && a.y() == -b.y();
}

auto boardIndex(const QPoint& point, const int width) -> int {
  return point.y() * width + point.x();
}

auto mixHash(std::uint64_t seed, const std::uint64_t value) -> std::uint64_t {
  constexpr std::uint64_t kPrime = 1099511628211ULL;
  seed ^= value + 0x9e3779b97f4a7c15ULL + (seed << 6U) + (seed >> 2U);
  seed *= kPrime;
  return seed;
}

auto clampedFloat(const QJsonObject& object, const QString& key, const float fallback) -> float {
  const auto value = object.value(key);
  if (!value.isDouble()) {
    return fallback;
  }
  return std::clamp(static_cast<float>(value.toDouble()), 0.0F, 10.0F);
}

auto clampedInt(const QJsonObject& object, const QString& key, const int fallback) -> int {
  const auto value = object.value(key);
  if (!value.isDouble()) {
    return fallback;
  }
  return std::clamp(value.toInt(), 0, 1000000);
}

auto wrappedAxisDistance(const int a, const int b, const int size) -> int {
  if (size <= 0) {
    return std::abs(a - b);
  }
  const int delta = std::abs(a - b);
  return std::min(delta, size - delta);
}

auto wrappedManhattanDistance(const QPoint& from, const QPoint& to, const int width, const int height)
  -> int {
  return wrappedAxisDistance(from.x(), to.x(), width) + wrappedAxisDistance(from.y(), to.y(), height);
}

auto boardCenter(const Snapshot& snapshot) -> QPoint {
  return QPoint(snapshot.boardWidth / 2, snapshot.boardHeight / 2);
}

auto isPointInCenterBand(const QPoint& point, const Snapshot& snapshot) -> bool {
  if (snapshot.boardWidth <= 0 || snapshot.boardHeight <= 0) {
    return false;
  }
  const int marginX = std::max(2, snapshot.boardWidth / 4);
  const int marginY = std::max(2, snapshot.boardHeight / 4);
  const int minX = marginX;
  const int maxX = std::max(minX, snapshot.boardWidth - 1 - marginX);
  const int minY = marginY;
  const int maxY = std::max(minY, snapshot.boardHeight - 1 - marginY);
  return point.x() >= minX && point.x() <= maxX && point.y() >= minY && point.y() <= maxY;
}

auto cornerDistance(const QPoint& point, const Snapshot& snapshot) -> int {
  const std::array<QPoint, 4> corners = {
    QPoint(0, 0),
    QPoint(snapshot.boardWidth - 1, 0),
    QPoint(snapshot.boardWidth - 1, snapshot.boardHeight - 1),
    QPoint(0, snapshot.boardHeight - 1),
  };
  int best = std::numeric_limits<int>::max();
  for (const QPoint& corner : corners) {
    best = std::min(
      best,
      wrappedManhattanDistance(point, corner, snapshot.boardWidth, snapshot.boardHeight));
  }
  return best;
}

struct CandidateMetrics {
  QPoint direction{0, 0};
  QPoint nextHead{0, 0};
  std::deque<QPoint> nextBody;
  int openSpace = 0;
  int safeNeighbors = 0;
  int repeats = 0;
  int orbitRepeats = 0;
  int foodDistance = 0;
  std::uint64_t hash = 0;
  std::uint64_t orbitHash = 0;
  float logit = 0.0F;
  float score = std::numeric_limits<float>::lowest();
  int tieRank = 0;
};

} // namespace

void MlBackend::setConfidenceGate(const float minConfidence, const float minMargin) {
  m_minConfidence = std::clamp(minConfidence, 0.0F, 1.0F);
  m_minMargin = std::max(0.0F, minMargin);
}

void MlBackend::reset() {
  m_recentHashes.clear();
  m_hashCounts.clear();
  m_recentOrbitHashes.clear();
  m_orbitCounts.clear();
  m_lastScore = 0;
  m_noScoreTicks = 0;
  m_hasScore = false;
  m_lastFoodDistance = 0;
  m_noProgressTicks = 0;
  m_hasFoodDistance = false;
  m_recentDirections.clear();
  m_recentEscapeLikeMoves.clear();
}

auto MlBackend::loadFromFile(const QString& path) -> bool {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    return markUnavailable(QStringLiteral("open failed: %1").arg(path));
  }
  return loadFromJson(file.readAll(), path);
}

auto MlBackend::markUnavailable(const QString& error) -> bool {
  m_available = false;
  m_error = error;
  m_source.clear();
  m_layers.clear();
  reset();
  return false;
}

auto MlBackend::loadFromJson(const QByteArray& jsonBytes, const QString& sourceLabel) -> bool {
  QJsonParseError parseError{};
  const auto document = QJsonDocument::fromJson(jsonBytes, &parseError);
  if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
    return markUnavailable(QStringLiteral("invalid json: %1").arg(parseError.errorString()));
  }

  const auto root = document.object();
  const auto format = root.value(QStringLiteral("format")).toString();
  if (format != QStringLiteral("nenoserpent-bot-mlp-v2")) {
    return markUnavailable(QStringLiteral("unsupported format: %1").arg(format));
  }
  const auto featureColumns = root.value(QStringLiteral("feature_columns_v2"));
  if (!featureColumns.isUndefined() &&
      (!featureColumns.isArray() ||
       featureColumns.toArray().size() != static_cast<int>(Features::kSize))) {
    return markUnavailable(QStringLiteral("invalid feature_columns_v2"));
  }

  const auto normalization = root.value(QStringLiteral("normalization")).toObject();
  std::vector<float> meanVec;
  std::vector<float> stdVec;
  if (!parseFloatArray(normalization.value(QStringLiteral("mean")), Features::kSize, meanVec) ||
      !parseFloatArray(normalization.value(QStringLiteral("std")), Features::kSize, stdVec)) {
    return markUnavailable(QStringLiteral("invalid normalization arrays"));
  }
  std::ranges::copy(meanVec, m_mean.begin());
  std::ranges::copy(stdVec, m_std.begin());
  for (float& value : m_std) {
    if (std::abs(value) < 1.0e-6F) {
      value = 1.0F;
    }
  }

  const auto layersValue = root.value(QStringLiteral("layers"));
  if (!layersValue.isArray()) {
    return markUnavailable(QStringLiteral("missing layers array"));
  }
  const auto layersArray = layersValue.toArray();
  if (layersArray.isEmpty()) {
    return markUnavailable(QStringLiteral("empty layers"));
  }

  std::vector<Layer> parsedLayers;
  parsedLayers.reserve(static_cast<std::size_t>(layersArray.size()));

  int expectedInputDim = Features::kSize;
  for (const auto layerValue : layersArray) {
    if (!layerValue.isObject()) {
      return markUnavailable(QStringLiteral("layer entry must be object"));
    }
    const auto object = layerValue.toObject();
    const int inputDim = object.value(QStringLiteral("input_dim")).toInt();
    const int outputDim = object.value(QStringLiteral("output_dim")).toInt();
    if (inputDim <= 0 || outputDim <= 0) {
      return markUnavailable(QStringLiteral("invalid layer dimensions"));
    }
    if (inputDim != expectedInputDim) {
      return markUnavailable(QStringLiteral("layer input mismatch"));
    }

    const QString activationText = object.value(QStringLiteral("activation"))
                                     .toString(QStringLiteral("none"))
                                     .trimmed()
                                     .toLower();
    Activation activation = Activation::None;
    if (activationText == QStringLiteral("relu")) {
      activation = Activation::Relu;
    } else if (activationText != QStringLiteral("none")) {
      return markUnavailable(QStringLiteral("unsupported activation: %1").arg(activationText));
    }

    Layer layer{};
    layer.inputDim = inputDim;
    layer.outputDim = outputDim;
    layer.activation = activation;

    const int weightCount = inputDim * outputDim;
    if (!parseFloatArray(object.value(QStringLiteral("weights")), weightCount, layer.weights) ||
        !parseFloatArray(object.value(QStringLiteral("bias")), outputDim, layer.bias)) {
      return markUnavailable(QStringLiteral("invalid weights or bias array"));
    }
    parsedLayers.push_back(std::move(layer));
    expectedInputDim = outputDim;
  }

  if (expectedInputDim != 4) {
    return markUnavailable(QStringLiteral("final output dim must be 4"));
  }

  const auto hybrid = root.value(QStringLiteral("hybrid")).toObject();
  m_hybridConfig.logitWeight = clampedFloat(hybrid, QStringLiteral("logit_weight"), 1.0F);
  m_hybridConfig.riskWeight = clampedFloat(hybrid, QStringLiteral("risk_weight"), 0.9F);
  m_hybridConfig.loopWeight = clampedFloat(hybrid, QStringLiteral("loop_weight"), 0.8F);
  m_hybridConfig.orbitWeight = clampedFloat(hybrid, QStringLiteral("orbit_weight"), 1.15F);
  m_hybridConfig.stallWeight = clampedFloat(hybrid, QStringLiteral("stall_weight"), 0.45F);
  m_hybridConfig.progressWeight = clampedFloat(hybrid, QStringLiteral("progress_weight"), 0.45F);
  m_hybridConfig.foodWeight = clampedFloat(hybrid, QStringLiteral("food_weight"), 0.35F);
  m_hybridConfig.spaceWeight = clampedFloat(hybrid, QStringLiteral("space_weight"), 0.16F);
  m_hybridConfig.safeNeighborWeight =
    clampedFloat(hybrid, QStringLiteral("safe_neighbor_weight"), 0.12F);
  m_hybridConfig.hashWindow = std::clamp(clampedInt(hybrid, QStringLiteral("hash_window"), 192), 64, 512);
  m_hybridConfig.tieBreakSeed = clampedInt(hybrid, QStringLiteral("tie_break_seed"), 17);

  m_source = sourceLabel;
  m_layers = std::move(parsedLayers);
  m_available = true;
  m_error.clear();
  reset();
  return true;
}

auto MlBackend::normalizedFeature(const Snapshot& snapshot) const -> std::array<float, 21> {
  auto feature = extractFeatures(snapshot).values;
  for (std::size_t i = 0; i < static_cast<std::size_t>(Features::kSize); ++i) {
    feature[i] = (feature[i] - m_mean[i]) / m_std[i];
  }
  return feature;
}

auto MlBackend::inferLogits(const Snapshot& snapshot) const -> std::optional<std::array<float, 4>> {
  if (!m_available || m_layers.empty()) {
    return std::nullopt;
  }

  std::vector<float> input;
  input.reserve(Features::kSize);
  const auto normalized = normalizedFeature(snapshot);
  input.assign(normalized.begin(), normalized.end());

  for (const auto& layer : m_layers) {
    std::vector<float> output(static_cast<std::size_t>(layer.outputDim), 0.0F);
    for (int row = 0; row < layer.outputDim; ++row) {
      float value = layer.bias[static_cast<std::size_t>(row)];
      for (int col = 0; col < layer.inputDim; ++col) {
        const std::size_t weightIndex =
          static_cast<std::size_t>(row) * static_cast<std::size_t>(layer.inputDim) +
          static_cast<std::size_t>(col);
        value += layer.weights[weightIndex] * input[static_cast<std::size_t>(col)];
      }
      if (layer.activation == Activation::Relu) {
        value = std::max(0.0F, value);
      }
      output[static_cast<std::size_t>(row)] = value;
    }
    input = std::move(output);
  }

  if (input.size() != 4) {
    return std::nullopt;
  }
  return std::array<float, 4>{input[0], input[1], input[2], input[3]};
}

auto MlBackend::passesConfidenceGate(const std::array<float, 4>& logits) const -> bool {
  const float maxLogit = *std::ranges::max_element(logits);
  std::array<float, 4> exps{};
  float sum = 0.0F;
  for (std::size_t i = 0; i < logits.size(); ++i) {
    exps[i] = std::exp(logits[i] - maxLogit);
    sum += exps[i];
  }
  if (sum <= 0.0F) {
    return false;
  }

  std::array<int, 4> indices = {0, 1, 2, 3};
  std::ranges::sort(indices, [&](const int a, const int b) {
    return logits[static_cast<std::size_t>(a)] > logits[static_cast<std::size_t>(b)];
  });
  const int bestIndex = indices[0];
  const int secondIndex = indices[1];
  const float confidence = exps[static_cast<std::size_t>(bestIndex)] / sum;
  const float margin =
    logits[static_cast<std::size_t>(bestIndex)] - logits[static_cast<std::size_t>(secondIndex)];
  return confidence >= m_minConfidence && margin >= m_minMargin;
}

auto MlBackend::isDirectionAllowed(const Snapshot& snapshot, const QPoint& candidate) const
  -> bool {
  if (snapshot.boardWidth <= 0 || snapshot.boardHeight <= 0) {
    return false;
  }
  if (isReverseDirection(candidate, snapshot.direction)) {
    return false;
  }

  const QPoint nextHeadRaw = snapshot.head + candidate;
  const QPoint wrappedHead =
    nenoserpent::core::wrapPoint(nextHeadRaw, snapshot.boardWidth, snapshot.boardHeight);
  std::deque<QPoint> collisionBody = snapshot.body;
  const bool wouldEatFood = wrappedHead == snapshot.food;
  if (!wouldEatFood && !collisionBody.empty()) {
    collisionBody.pop_back();
  }
  const auto collision = nenoserpent::core::collisionOutcomeForHead(nextHeadRaw,
                                                                    snapshot.boardWidth,
                                                                    snapshot.boardHeight,
                                                                    snapshot.obstacles,
                                                                    collisionBody,
                                                                    snapshot.ghostActive,
                                                                    snapshot.portalActive,
                                                                    snapshot.laserActive,
                                                                    snapshot.shieldActive);
  return !collision.collision;
}

auto MlBackend::stateHash(const Snapshot& snapshot,
                          const QPoint& head,
                          const QPoint& direction,
                          const std::deque<QPoint>& body) const -> std::uint64_t {
  std::uint64_t hash = 1469598103934665603ULL;
  hash = mixHash(hash, static_cast<std::uint64_t>(snapshot.boardWidth));
  hash = mixHash(hash, static_cast<std::uint64_t>(snapshot.boardHeight));
  hash = mixHash(hash, static_cast<std::uint64_t>(head.x() + 2048));
  hash = mixHash(hash, static_cast<std::uint64_t>(head.y() + 2048));
  hash = mixHash(hash, static_cast<std::uint64_t>(direction.x() + 16));
  hash = mixHash(hash, static_cast<std::uint64_t>(direction.y() + 16));
  hash = mixHash(hash, static_cast<std::uint64_t>(snapshot.food.x() + 2048));
  hash = mixHash(hash, static_cast<std::uint64_t>(snapshot.food.y() + 2048));
  hash = mixHash(hash, static_cast<std::uint64_t>(snapshot.powerUpPos.x() + 2048));
  hash = mixHash(hash, static_cast<std::uint64_t>(snapshot.powerUpPos.y() + 2048));
  hash = mixHash(hash, static_cast<std::uint64_t>(snapshot.powerUpType + 64));
  hash = mixHash(hash, static_cast<std::uint64_t>(body.size()));
  for (const QPoint& segment : body) {
    hash = mixHash(hash, static_cast<std::uint64_t>(segment.x() + 2048));
    hash = mixHash(hash, static_cast<std::uint64_t>(segment.y() + 2048));
  }
  return hash;
}

auto MlBackend::loopRepeatsFor(const std::uint64_t hash) const -> int {
  const auto it = m_hashCounts.find(hash);
  return it == m_hashCounts.end() ? 0 : it->second;
}

auto MlBackend::orbitRepeatsFor(const std::uint64_t hash) const -> int {
  const auto it = m_orbitCounts.find(hash);
  return it == m_orbitCounts.end() ? 0 : it->second;
}

auto MlBackend::observeStateHash(const std::uint64_t hash) const -> void {
  ++m_hashCounts[hash];
  m_recentHashes.push_back(hash);
  while (static_cast<int>(m_recentHashes.size()) > m_hybridConfig.hashWindow) {
    const std::uint64_t dropped = m_recentHashes.front();
    m_recentHashes.pop_front();
    auto it = m_hashCounts.find(dropped);
    if (it == m_hashCounts.end()) {
      continue;
    }
    --it->second;
    if (it->second <= 0) {
      m_hashCounts.erase(it);
    }
  }
}

auto MlBackend::observeOrbitHash(const std::uint64_t hash) const -> void {
  ++m_orbitCounts[hash];
  m_recentOrbitHashes.push_back(hash);
  while (static_cast<int>(m_recentOrbitHashes.size()) > m_hybridConfig.hashWindow) {
    const std::uint64_t dropped = m_recentOrbitHashes.front();
    m_recentOrbitHashes.pop_front();
    auto it = m_orbitCounts.find(dropped);
    if (it == m_orbitCounts.end()) {
      continue;
    }
    --it->second;
    if (it->second <= 0) {
      m_orbitCounts.erase(it);
    }
  }
}

auto MlBackend::observeScore(const int score) const -> int {
  if (!m_hasScore || score > m_lastScore) {
    m_noScoreTicks = 0;
  } else {
    ++m_noScoreTicks;
  }
  m_lastScore = score;
  m_hasScore = true;
  return m_noScoreTicks;
}

auto MlBackend::observeFoodDistance(const int foodDistance) const -> int {
  if (!m_hasFoodDistance || foodDistance < m_lastFoodDistance) {
    m_noProgressTicks = 0;
  } else {
    ++m_noProgressTicks;
  }
  m_lastFoodDistance = foodDistance;
  m_hasFoodDistance = true;
  return m_noProgressTicks;
}

auto MlBackend::decideDirection(const Snapshot& snapshot, const StrategyConfig& config) const
  -> std::optional<QPoint> {
  Q_UNUSED(config);
  if (snapshot.body.empty() || snapshot.boardWidth <= 0 || snapshot.boardHeight <= 0) {
    return std::nullopt;
  }
  const auto logits = inferLogits(snapshot);
  if (!logits.has_value()) {
    return std::nullopt;
  }
  if (!passesConfidenceGate(*logits)) {
    return std::nullopt;
  }

  const int noScoreTicks = observeScore(snapshot.score);
  const int currentFoodDistance =
    wrappedManhattanDistance(snapshot.head, snapshot.food, snapshot.boardWidth, snapshot.boardHeight);
  const int noProgressTicks = observeFoodDistance(currentFoodDistance);
  const int boardArea = std::max(1, snapshot.boardWidth * snapshot.boardHeight);
  const int maxFoodDistance = std::max(1, (snapshot.boardWidth / 2) + (snapshot.boardHeight / 2));

  auto buildBlockedMap = [&](const std::deque<QPoint>& body) -> std::vector<bool> {
    std::vector<bool> blocked(static_cast<std::size_t>(boardArea), false);
    if (!snapshot.portalActive && !snapshot.laserActive) {
      for (const QPoint& obstacle : snapshot.obstacles) {
        blocked[static_cast<std::size_t>(boardIndex(obstacle, snapshot.boardWidth))] = true;
      }
    }
    if (!snapshot.ghostActive) {
      for (const QPoint& segment : body) {
        blocked[static_cast<std::size_t>(boardIndex(segment, snapshot.boardWidth))] = true;
      }
    }
    return blocked;
  };

  auto floodReachable = [&](const QPoint& start, const std::vector<bool>& blocked) -> int {
    std::vector<bool> visited(blocked.size(), false);
    std::deque<QPoint> queue;
    queue.push_back(start);
    visited[static_cast<std::size_t>(boardIndex(start, snapshot.boardWidth))] = true;
    int reachable = 0;
    while (!queue.empty()) {
      const QPoint current = queue.front();
      queue.pop_front();
      ++reachable;
      for (const QPoint& dir : kDirections) {
        const QPoint next =
          nenoserpent::core::wrapPoint(current + dir, snapshot.boardWidth, snapshot.boardHeight);
        const std::size_t idx = static_cast<std::size_t>(boardIndex(next, snapshot.boardWidth));
        if (visited[idx] || blocked[idx]) {
          continue;
        }
        visited[idx] = true;
        queue.push_back(next);
      }
    }
    return reachable;
  };

  auto countSafeNeighbors = [&](const QPoint& from, const std::vector<bool>& blocked) -> int {
    int safe = 0;
    for (const QPoint& dir : kDirections) {
      const QPoint next =
        nenoserpent::core::wrapPoint(from + dir, snapshot.boardWidth, snapshot.boardHeight);
      const std::size_t idx = static_cast<std::size_t>(boardIndex(next, snapshot.boardWidth));
      if (!blocked[idx]) {
        ++safe;
      }
    }
    return safe;
  };

  const QPoint center = boardCenter(snapshot);
  const bool centerFoodPush = isPointInCenterBand(snapshot.food, snapshot);
  int escapeLikeCount = 0;
  for (const bool value : m_recentEscapeLikeMoves) {
    if (value) {
      ++escapeLikeCount;
    }
  }
  const float escapeLikeRatio =
    m_recentEscapeLikeMoves.empty()
      ? 0.0F
      : static_cast<float>(escapeLikeCount) / static_cast<float>(m_recentEscapeLikeMoves.size());

  auto directionStreakPenalty = [&](const QPoint& direction, const int noScore) -> float {
    if (m_recentDirections.empty()) {
      return 0.0F;
    }
    int streak = 0;
    for (auto it = m_recentDirections.rbegin(); it != m_recentDirections.rend(); ++it) {
      if (*it != direction) {
        break;
      }
      ++streak;
    }
    if (streak < 2) {
      return 0.0F;
    }
    return static_cast<float>(streak - 1) * (0.42F + (static_cast<float>(noScore) / 180.0F));
  };

  auto periodicCyclePenalty = [&](const QPoint& direction, const int period, const float factor)
    -> float {
    const int n = static_cast<int>(m_recentDirections.size());
    if (n < (period * 2) - 1) {
      return 0.0F;
    }
    for (int i = 0; i < period; ++i) {
      const QPoint lhs =
        (i == period - 1) ? direction : m_recentDirections[static_cast<std::size_t>(n - period + i)];
      const QPoint rhs = m_recentDirections[static_cast<std::size_t>(n - (period * 2) + i + 1)];
      if (lhs != rhs) {
        return 0.0F;
      }
    }
    return factor;
  };

  std::array<CandidateMetrics, 4> candidates{};
  int candidateCount = 0;

  for (int index = 0; index < static_cast<int>(kDirections.size()); ++index) {
    const auto candidate = classDirection(index);
    if (!candidate.has_value() || isReverseDirection(*candidate, snapshot.direction)) {
      continue;
    }
    const QPoint nextHeadRaw = snapshot.head + *candidate;
    const QPoint wrappedHead =
      nenoserpent::core::wrapPoint(nextHeadRaw, snapshot.boardWidth, snapshot.boardHeight);
    std::deque<QPoint> collisionBody = snapshot.body;
    const bool wouldEatFood = wrappedHead == snapshot.food;
    if (!wouldEatFood && !collisionBody.empty()) {
      collisionBody.pop_back();
    }
    const auto collision = nenoserpent::core::collisionOutcomeForHead(nextHeadRaw,
                                                                      snapshot.boardWidth,
                                                                      snapshot.boardHeight,
                                                                      snapshot.obstacles,
                                                                      collisionBody,
                                                                      snapshot.ghostActive,
                                                                      snapshot.portalActive,
                                                                      snapshot.laserActive,
                                                                      snapshot.shieldActive);
    if (collision.collision) {
      continue;
    }
    std::deque<QPoint> nextBody = collisionBody;
    nextBody.push_front(wrappedHead);

    auto blocked = buildBlockedMap(nextBody);
    blocked[static_cast<std::size_t>(boardIndex(wrappedHead, snapshot.boardWidth))] = false;
    const int openSpace = floodReachable(wrappedHead, blocked);
    const int safeNeighbors = countSafeNeighbors(wrappedHead, blocked);

    CandidateMetrics metrics{};
    metrics.direction = *candidate;
    metrics.nextHead = wrappedHead;
    metrics.nextBody = std::move(nextBody);
    metrics.openSpace = openSpace;
    metrics.safeNeighbors = safeNeighbors;
    metrics.logit = logits->at(static_cast<std::size_t>(index));
    metrics.hash = stateHash(snapshot, wrappedHead, *candidate, metrics.nextBody);
    metrics.repeats = loopRepeatsFor(metrics.hash);
    metrics.foodDistance = wrappedManhattanDistance(wrappedHead,
                                                    snapshot.food,
                                                    snapshot.boardWidth,
                                                    snapshot.boardHeight);
    metrics.orbitHash = stateHash(snapshot, wrappedHead, *candidate, {});
    metrics.orbitRepeats = orbitRepeatsFor(metrics.orbitHash);
    metrics.tieRank = (index + m_hybridConfig.tieBreakSeed) % static_cast<int>(kDirections.size());

    const float openPct = static_cast<float>(openSpace) / static_cast<float>(boardArea);
    float risk = 0.0F;
    if (safeNeighbors <= 1) {
      risk += 1.6F;
    } else if (safeNeighbors == 2) {
      risk += 0.7F;
    }
    if (openSpace < static_cast<int>(metrics.nextBody.size()) + 8) {
      risk += 1.2F;
    } else if (openPct < 0.18F) {
      risk += 0.8F;
    }
    const float loopPenalty = static_cast<float>(metrics.repeats * metrics.repeats);
    const float orbitPenalty = static_cast<float>(metrics.orbitRepeats * metrics.orbitRepeats);
    const float repeatPenalty = directionStreakPenalty(metrics.direction, noScoreTicks);
    const float cyclePenalty4 = periodicCyclePenalty(metrics.direction, 4, 2.6F);
    const float cyclePenalty8 = periodicCyclePenalty(metrics.direction, 8, 3.4F);
    const float stallPenalty =
      noScoreTicks > 20 ? static_cast<float>(noScoreTicks - 20) / 12.0F : 0.0F;
    const float progressPenalty =
      noProgressTicks > 16 ? static_cast<float>(noProgressTicks - 16) / 10.0F : 0.0F;
    const float foodReward =
      1.0F - (static_cast<float>(metrics.foodDistance) / static_cast<float>(maxFoodDistance));
    const float distanceDelta =
      static_cast<float>(currentFoodDistance - metrics.foodDistance);
    const float centerDelta = static_cast<float>(wrappedManhattanDistance(snapshot.head,
                                                                          center,
                                                                          snapshot.boardWidth,
                                                                          snapshot.boardHeight) -
                                                  wrappedManhattanDistance(metrics.nextHead,
                                                                          center,
                                                                          snapshot.boardWidth,
                                                                          snapshot.boardHeight));
    const bool leaveCorner = cornerDistance(snapshot.head, snapshot) <= 3 &&
                             cornerDistance(metrics.nextHead, snapshot) > 3;
    const bool moveToCorner = cornerDistance(snapshot.head, snapshot) > 3 &&
                              cornerDistance(metrics.nextHead, snapshot) <= 3;
    const bool escapeLike = !wouldEatFood && metrics.foodDistance >= currentFoodDistance;
    const float escapeSharePenalty =
      (escapeLike && escapeLikeRatio >= 0.68F)
        ? (0.8F + ((escapeLikeRatio - 0.68F) * 3.2F))
        : 0.0F;

    metrics.score = (m_hybridConfig.logitWeight * metrics.logit) -
                    (m_hybridConfig.riskWeight * risk) -
                    (m_hybridConfig.loopWeight * loopPenalty) -
                    (m_hybridConfig.orbitWeight * orbitPenalty) -
                    repeatPenalty - cyclePenalty4 - cyclePenalty8 - escapeSharePenalty -
                    (m_hybridConfig.stallWeight * stallPenalty) -
                    (m_hybridConfig.progressWeight * progressPenalty) +
                    (m_hybridConfig.progressWeight * distanceDelta) +
                    (m_hybridConfig.foodWeight * foodReward) +
                    (m_hybridConfig.spaceWeight * (openPct * 10.0F)) +
                    (m_hybridConfig.safeNeighborWeight * static_cast<float>(safeNeighbors));
    if (noScoreTicks >= 24 && distanceDelta <= 0.0F) {
      metrics.score -= std::min(6.2F, static_cast<float>(noScoreTicks - 24) / 10.0F);
    }
    if (centerFoodPush) {
      metrics.score += centerDelta * 0.6F;
      if (leaveCorner) {
        metrics.score += 1.9F;
      }
      if (moveToCorner) {
        metrics.score -= 2.1F;
      }
    }
    candidates[static_cast<std::size_t>(candidateCount)] = std::move(metrics);
    ++candidateCount;
  }
  if (candidateCount <= 0) {
    return std::nullopt;
  }

  const CandidateMetrics* best = &candidates[0];
  for (int i = 1; i < candidateCount; ++i) {
    const CandidateMetrics* current = &candidates[static_cast<std::size_t>(i)];
    if (current->score > best->score ||
        (std::abs(current->score - best->score) < 1.0e-6F && current->tieRank < best->tieRank)) {
      best = current;
    }
  }
  observeStateHash(best->hash);
  observeOrbitHash(best->orbitHash);
  m_recentDirections.push_back(best->direction);
  while (m_recentDirections.size() > 24) {
    m_recentDirections.pop_front();
  }
  const bool bestEscapeLike = best->foodDistance >= currentFoodDistance;
  m_recentEscapeLikeMoves.push_back(bestEscapeLike);
  while (m_recentEscapeLikeMoves.size() > 128) {
    m_recentEscapeLikeMoves.pop_front();
  }
  return best->direction;
}

auto MlBackend::decideChoice(const QVariantList& choices, const StrategyConfig& config) const
  -> int {
  return pickChoiceIndex(choices, config);
}

} // namespace nenoserpent::adapter::bot
