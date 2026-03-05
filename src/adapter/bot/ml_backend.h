#pragma once

#include <array>
#include <optional>
#include <vector>

#include <QByteArray>
#include <QString>

#include "adapter/bot/backend.h"

namespace nenoserpent::adapter::bot {

class MlBackend final : public BotBackend {
public:
  MlBackend() = default;

  [[nodiscard]] auto name() const -> QString override {
    return QStringLiteral("ml");
  }
  [[nodiscard]] auto isAvailable() const -> bool override {
    return m_available;
  }
  [[nodiscard]] auto errorString() const -> const QString& {
    return m_error;
  }
  [[nodiscard]] auto source() const -> const QString& {
    return m_source;
  }
  void setConfidenceGate(float minConfidence, float minMargin);

  auto loadFromFile(const QString& path) -> bool;
  auto loadFromJson(const QByteArray& jsonBytes, const QString& sourceLabel) -> bool;
  void reset() override {
  }

  [[nodiscard]] auto decideDirection(const Snapshot& snapshot, const StrategyConfig& config) const
    -> std::optional<QPoint> override;
  [[nodiscard]] auto decideChoice(const QVariantList& choices, const StrategyConfig& config) const
    -> int override;

private:
  enum class Activation { None, Relu };

  struct Layer {
    int inputDim = 0;
    int outputDim = 0;
    Activation activation = Activation::None;
    std::vector<float> weights;
    std::vector<float> bias;
  };

  auto markUnavailable(const QString& error) -> bool;
  [[nodiscard]] auto inferLogits(const Snapshot& snapshot) const
    -> std::optional<std::array<float, 4>>;
  [[nodiscard]] auto passesConfidenceGate(const std::array<float, 4>& logits) const -> bool;
  [[nodiscard]] auto isDirectionAllowed(const Snapshot& snapshot, const QPoint& candidate) const
    -> bool;
  [[nodiscard]] auto normalizedFeature(const Snapshot& snapshot) const -> std::array<float, 21>;

  bool m_available = false;
  QString m_error;
  QString m_source;
  std::array<float, 21> m_mean{};
  std::array<float, 21> m_std{};
  std::vector<Layer> m_layers;
  float m_minConfidence = 0.90F;
  float m_minMargin = 1.20F;
};

} // namespace nenoserpent::adapter::bot
