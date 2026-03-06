#pragma once

#include <QObject>
#include <QPoint>
#include <QPointF>
#include <QVariantList>

#include "app_state.h"

class EngineAdapter;
class SnakeModel;

class SessionRenderViewModel final : public QObject {
  Q_OBJECT
  Q_PROPERTY(SnakeModel* snakeModel READ snakeModel CONSTANT)
  Q_PROPERTY(QPoint food READ food NOTIFY foodChanged)
  Q_PROPERTY(QPoint powerUpPos READ powerUpPos NOTIFY powerUpChanged)
  Q_PROPERTY(int powerUpType READ powerUpType NOTIFY powerUpChanged)
  Q_PROPERTY(int score READ score NOTIFY scoreChanged)
  Q_PROPERTY(int highScore READ highScore NOTIFY highScoreChanged)
  Q_PROPERTY(AppState::Value state READ state NOTIFY stateChanged)
  Q_PROPERTY(int boardWidth READ boardWidth CONSTANT)
  Q_PROPERTY(int boardHeight READ boardHeight CONSTANT)
  Q_PROPERTY(QVariantList obstacles READ obstacles NOTIFY obstaclesChanged)
  Q_PROPERTY(QVariantList ghost READ ghost NOTIFY ghostChanged)
  Q_PROPERTY(int activeBuff READ activeBuff NOTIFY buffChanged)
  Q_PROPERTY(int buffTicksRemaining READ buffTicksRemaining NOTIFY buffChanged)
  Q_PROPERTY(int buffTicksTotal READ buffTicksTotal NOTIFY buffChanged)
  Q_PROPERTY(QPointF reflectionOffset READ reflectionOffset NOTIFY reflectionOffsetChanged)
  Q_PROPERTY(bool shieldActive READ shieldActive NOTIFY buffChanged)
  Q_PROPERTY(QPoint scoutHintCell READ scoutHintCell NOTIFY buffChanged)

public:
  explicit SessionRenderViewModel(EngineAdapter* engineAdapter, QObject* parent = nullptr);

  [[nodiscard]] auto snakeModel() const -> SnakeModel*;
  [[nodiscard]] auto food() const -> QPoint;
  [[nodiscard]] auto powerUpPos() const -> QPoint;
  [[nodiscard]] auto powerUpType() const -> int;
  [[nodiscard]] auto score() const -> int;
  [[nodiscard]] auto highScore() const -> int;
  [[nodiscard]] auto state() const -> AppState::Value;
  [[nodiscard]] auto boardWidth() const -> int;
  [[nodiscard]] auto boardHeight() const -> int;
  [[nodiscard]] auto obstacles() const -> QVariantList;
  [[nodiscard]] auto ghost() const -> QVariantList;
  [[nodiscard]] auto activeBuff() const -> int;
  [[nodiscard]] auto buffTicksRemaining() const -> int;
  [[nodiscard]] auto buffTicksTotal() const -> int;
  [[nodiscard]] auto reflectionOffset() const -> QPointF;
  [[nodiscard]] auto shieldActive() const -> bool;
  [[nodiscard]] auto scoutHintCell() const -> QPoint;

signals:
  void foodChanged();
  void powerUpChanged();
  void scoreChanged();
  void highScoreChanged();
  void stateChanged();
  void obstaclesChanged();
  void ghostChanged();
  void buffChanged();
  void reflectionOffsetChanged();

private:
  EngineAdapter* m_engineAdapter = nullptr;
};
