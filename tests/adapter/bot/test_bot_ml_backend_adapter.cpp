#include <QByteArray>
#include <QtTest/QtTest>

#include "adapter/bot/ml_backend.h"

class BotMlBackendAdapterTest final : public QObject {
  Q_OBJECT

private slots:
  void rejectsInvalidModelJson();
  void picksDirectionFromLoadedModel();
  void skipsReverseDirectionCandidate();
  void returnsNulloptWhenConfidenceGateRejects();
  void allowsShieldCollisionCandidate();
  void allowsPortalAndLaserObstacleCandidate();
};

void BotMlBackendAdapterTest::rejectsInvalidModelJson() {
  nenoserpent::adapter::bot::MlBackend backend;
  QVERIFY(!backend.loadFromJson("{\"format\":\"invalid\"}", QStringLiteral("inline")));
  QVERIFY(!backend.isAvailable());
}

void BotMlBackendAdapterTest::picksDirectionFromLoadedModel() {
  const QByteArray modelJson = R"({
    "format": "nenoserpent-bot-mlp-v2",
    "normalization": {
      "mean": [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0],
      "std": [1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1]
    },
    "layers": [{
      "input_dim": 21,
      "output_dim": 4,
      "activation": "none",
      "weights": [
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,5,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,-5,0,0,0,0,0,0,0,0,0,0,0,0,0
      ],
      "bias": [0,0,0,0]
    }]
  })";

  nenoserpent::adapter::bot::MlBackend backend;
  QVERIFY(backend.loadFromJson(modelJson, QStringLiteral("inline")));
  backend.setConfidenceGate(0.0F, 0.0F);
  QVERIFY(backend.isAvailable());

  nenoserpent::adapter::bot::Snapshot snapshot{};
  snapshot.head = QPoint(10, 10);
  snapshot.direction = QPoint(0, -1);
  snapshot.food = QPoint(13, 10);
  snapshot.boardWidth = 20;
  snapshot.boardHeight = 18;
  snapshot.body = {QPoint(10, 10), QPoint(10, 11), QPoint(10, 12)};

  const auto result =
    backend.decideDirection(snapshot, nenoserpent::adapter::bot::defaultStrategyConfig());
  QVERIFY(result.has_value());
  QCOMPARE(*result, QPoint(1, 0));
}

void BotMlBackendAdapterTest::skipsReverseDirectionCandidate() {
  const QByteArray modelJson = R"({
    "format": "nenoserpent-bot-mlp-v2",
    "normalization": {
      "mean": [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0],
      "std": [1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1]
    },
    "layers": [{
      "input_dim": 21,
      "output_dim": 4,
      "activation": "none",
      "weights": [
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
      ],
      "bias": [5,10,0,1]
    }]
  })";

  nenoserpent::adapter::bot::MlBackend backend;
  QVERIFY(backend.loadFromJson(modelJson, QStringLiteral("inline")));
  backend.setConfidenceGate(0.0F, 0.0F);

  nenoserpent::adapter::bot::Snapshot snapshot{};
  snapshot.head = QPoint(10, 10);
  snapshot.direction = QPoint(-1, 0);
  snapshot.food = QPoint(13, 10);
  snapshot.boardWidth = 20;
  snapshot.boardHeight = 18;
  snapshot.body = {QPoint(10, 10), QPoint(11, 10), QPoint(12, 10)};

  const auto result =
    backend.decideDirection(snapshot, nenoserpent::adapter::bot::defaultStrategyConfig());
  QVERIFY(result.has_value());
  QCOMPARE(*result, QPoint(0, -1));
}

void BotMlBackendAdapterTest::returnsNulloptWhenConfidenceGateRejects() {
  const QByteArray modelJson = R"({
    "format": "nenoserpent-bot-mlp-v2",
    "normalization": {
      "mean": [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0],
      "std": [1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1]
    },
    "layers": [{
      "input_dim": 21,
      "output_dim": 4,
      "activation": "none",
      "weights": [
        1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0.95,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
      ],
      "bias": [0,0,0,0]
    }]
  })";

  nenoserpent::adapter::bot::MlBackend backend;
  QVERIFY(backend.loadFromJson(modelJson, QStringLiteral("inline")));
  backend.setConfidenceGate(0.75F, 0.2F);

  nenoserpent::adapter::bot::Snapshot snapshot{};
  snapshot.head = QPoint(10, 10);
  snapshot.direction = QPoint(0, -1);
  snapshot.food = QPoint(12, 10);
  snapshot.boardWidth = 20;
  snapshot.boardHeight = 18;
  snapshot.body = {QPoint(10, 10), QPoint(10, 11), QPoint(10, 12)};

  const auto result =
    backend.decideDirection(snapshot, nenoserpent::adapter::bot::defaultStrategyConfig());
  QVERIFY(!result.has_value());
}

void BotMlBackendAdapterTest::allowsShieldCollisionCandidate() {
  const QByteArray modelJson = R"({
    "format": "nenoserpent-bot-mlp-v2",
    "normalization": {
      "mean": [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0],
      "std": [1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1]
    },
    "layers": [{
      "input_dim": 21,
      "output_dim": 4,
      "activation": "none",
      "weights": [
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
      ],
      "bias": [0,8,0,0]
    }]
  })";

  nenoserpent::adapter::bot::MlBackend backend;
  QVERIFY(backend.loadFromJson(modelJson, QStringLiteral("inline")));
  backend.setConfidenceGate(0.0F, 0.0F);

  nenoserpent::adapter::bot::Snapshot snapshot{};
  snapshot.head = QPoint(10, 10);
  snapshot.direction = QPoint(0, -1);
  snapshot.food = QPoint(12, 10);
  snapshot.boardWidth = 20;
  snapshot.boardHeight = 18;
  snapshot.shieldActive = true;
  snapshot.obstacles = {QPoint(11, 10)};
  snapshot.body = {QPoint(10, 10), QPoint(10, 11), QPoint(10, 12)};

  const auto result =
    backend.decideDirection(snapshot, nenoserpent::adapter::bot::defaultStrategyConfig());
  QVERIFY(result.has_value());
  QCOMPARE(*result, QPoint(1, 0));
}

void BotMlBackendAdapterTest::allowsPortalAndLaserObstacleCandidate() {
  const QByteArray modelJson = R"({
    "format": "nenoserpent-bot-mlp-v2",
    "normalization": {
      "mean": [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0],
      "std": [1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1]
    },
    "layers": [{
      "input_dim": 21,
      "output_dim": 4,
      "activation": "none",
      "weights": [
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
      ],
      "bias": [0,8,0,0]
    }]
  })";

  nenoserpent::adapter::bot::MlBackend backend;
  QVERIFY(backend.loadFromJson(modelJson, QStringLiteral("inline")));
  backend.setConfidenceGate(0.0F, 0.0F);

  nenoserpent::adapter::bot::Snapshot portalSnapshot{};
  portalSnapshot.head = QPoint(10, 10);
  portalSnapshot.direction = QPoint(0, -1);
  portalSnapshot.food = QPoint(12, 10);
  portalSnapshot.boardWidth = 20;
  portalSnapshot.boardHeight = 18;
  portalSnapshot.portalActive = true;
  portalSnapshot.obstacles = {QPoint(11, 10)};
  portalSnapshot.body = {QPoint(10, 10), QPoint(10, 11), QPoint(10, 12)};

  const auto portalResult =
    backend.decideDirection(portalSnapshot, nenoserpent::adapter::bot::defaultStrategyConfig());
  QVERIFY(portalResult.has_value());
  QCOMPARE(*portalResult, QPoint(1, 0));

  nenoserpent::adapter::bot::Snapshot laserSnapshot = portalSnapshot;
  laserSnapshot.portalActive = false;
  laserSnapshot.laserActive = true;
  const auto laserResult =
    backend.decideDirection(laserSnapshot, nenoserpent::adapter::bot::defaultStrategyConfig());
  QVERIFY(laserResult.has_value());
  QCOMPARE(*laserResult, QPoint(1, 0));
}

QTEST_MAIN(BotMlBackendAdapterTest)
#include "test_bot_ml_backend_adapter.moc"
