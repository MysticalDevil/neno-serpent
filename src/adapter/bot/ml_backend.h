#pragma once

#include <array>
#include <cstdint>
#include <deque>
#include <optional>
#include <unordered_map>
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
  void reset() override;

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

  struct HybridConfig {
    float logitWeight = 1.0F;
    float riskWeight = 0.9F;
    float loopWeight = 0.8F;
    float orbitWeight = 1.15F;
    float stallWeight = 0.45F;
    float progressWeight = 0.45F;
    float foodWeight = 0.35F;
    float spaceWeight = 0.16F;
    float safeNeighborWeight = 0.12F;
    int hashWindow = 192;
    int tieBreakSeed = 17;
  };

  auto markUnavailable(const QString& error) -> bool;
  [[nodiscard]] auto inferLogits(const Snapshot& snapshot) const
    -> std::optional<std::array<float, 4>>;
  [[nodiscard]] auto passesConfidenceGate(const std::array<float, 4>& logits) const -> bool;
  [[nodiscard]] auto isDirectionAllowed(const Snapshot& snapshot, const QPoint& candidate) const
    -> bool;
  [[nodiscard]] auto stateHash(const Snapshot& snapshot,
                               const QPoint& head,
                               const QPoint& direction,
                               const std::deque<QPoint>& body) const -> std::uint64_t;
  [[nodiscard]] auto loopRepeatsFor(std::uint64_t hash) const -> int;
  [[nodiscard]] auto orbitRepeatsFor(std::uint64_t hash) const -> int;
  auto observeStateHash(std::uint64_t hash) const -> void;
  auto observeOrbitHash(std::uint64_t hash) const -> void;
  auto observeScore(int score) const -> int;
  auto observeFoodDistance(int foodDistance) const -> int;
  [[nodiscard]] auto normalizedFeature(const Snapshot& snapshot) const -> std::array<float, 21>;

  bool m_available = false;
  QString m_error;
  QString m_source;
  std::array<float, 21> m_mean{};
  std::array<float, 21> m_std{};
  std::vector<Layer> m_layers;
  HybridConfig m_hybridConfig{};
  float m_minConfidence = 0.55F;
  float m_minMargin = 0.10F;
  mutable std::deque<std::uint64_t> m_recentHashes;
  mutable std::unordered_map<std::uint64_t, int> m_hashCounts;
  mutable std::deque<std::uint64_t> m_recentOrbitHashes;
  mutable std::unordered_map<std::uint64_t, int> m_orbitCounts;
  mutable int m_lastScore = 0;
  mutable int m_noScoreTicks = 0;
  mutable bool m_hasScore = false;
  mutable int m_lastFoodDistance = 0;
  mutable int m_noProgressTicks = 0;
  mutable bool m_hasFoodDistance = false;
  mutable std::deque<QPoint> m_recentDirections;
  mutable std::deque<bool> m_recentEscapeLikeMoves;
};

} // namespace nenoserpent::adapter::bot
