#include "adapter/bot/ml_backend.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numeric>
#include <ranges>

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "adapter/bot/controller.h"
#include "adapter/bot/features.h"
#include "core/game/rules.h"

namespace nenoserpent::adapter::bot {

namespace {

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

} // namespace

void MlBackend::setConfidenceGate(const float minConfidence, const float minMargin) {
  m_minConfidence = std::clamp(minConfidence, 0.0F, 1.0F);
  m_minMargin = std::max(0.0F, minMargin);
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
  m_layers.clear();
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
  if (format != QStringLiteral("nenoserpent-bot-mlp-v1")) {
    return markUnavailable(QStringLiteral("unsupported format: %1").arg(format));
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

  m_source = sourceLabel;
  m_layers = std::move(parsedLayers);
  m_available = true;
  m_error.clear();
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
  if (isReverseDirection(candidate, snapshot.direction)) {
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

auto MlBackend::decideDirection(const Snapshot& snapshot, const StrategyConfig& config) const
  -> std::optional<QPoint> {
  Q_UNUSED(config);
  const auto logits = inferLogits(snapshot);
  if (!logits.has_value()) {
    return std::nullopt;
  }
  if (!passesConfidenceGate(*logits)) {
    return std::nullopt;
  }

  std::array<int, 4> indices = {0, 1, 2, 3};
  std::ranges::sort(indices, [&](const int a, const int b) {
    return logits->at(static_cast<std::size_t>(a)) > logits->at(static_cast<std::size_t>(b));
  });

  for (const int index : indices) {
    const auto candidate = classDirection(index);
    if (!candidate.has_value()) {
      continue;
    }
    if (isDirectionAllowed(snapshot, *candidate)) {
      return candidate;
    }
  }
  return std::nullopt;
}

auto MlBackend::decideChoice(const QVariantList& choices, const StrategyConfig& config) const
  -> int {
  return pickChoiceIndex(choices, config);
}

} // namespace nenoserpent::adapter::bot
