#include <deque>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest/QtTest>

#include "core/buff/runtime.h"
#include "core/choice/runtime.h"
#include "core/game/rules.h"
#include "core/level/runtime.h"
#include "core/replay/timeline.h"
#include "game_engine_interface.h"

// QtTest slot-based tests intentionally stay as member functions and use assertion-heavy bodies.
// The fake engine also keeps compact no-op overrides to focus these tests on replay/core behavior.
// NOLINTBEGIN(readability-convert-member-functions-to-static,readability-function-cognitive-complexity,readability-named-parameter,modernize-use-designated-initializers,bugprone-unchecked-optional-access)
class TestCoreRules : public QObject {
  Q_OBJECT

private slots:
  void testCollectFreeSpotsRespectsPredicate();
  void testPickRandomFreeSpotUsesProvidedIndexAndHandlesEdgeCases();
  void testMagnetCandidateSpotsPrioritizesXAxisWhenDistanceIsGreater();
  void testProbeCollisionRespectsGhostFlag();
  void testCollisionOutcomeMatchesPortalLaserAndShieldSemantics();
  void testTickIntervalForScoreUsesSpeedFloor();
  void testPickRoguelikeChoicesIsBoundedAndDeterministic();
  void testDynamicLevelFallbackProducesObstacles();
  void testWallsFromJsonArrayParsesCoordinates();
  void testResolvedLevelDataFromJsonMapsIndexAndFields();
  void testResolvedLevelDataFromJsonBytesParsesDocumentEnvelope();
  void testLevelCountFromJsonBytesUsesFallbackOnInvalidData();
  void testBuffRuntimeRules();
  void testTickBuffCountdown();
  void testWeightedRandomBuffIdUsesWeightsAndFallback();
  void testReplayTimelineAppliesOnlyOnMatchingTicks();
};

class FakeReplayEngine final : public IGameEngine {
public:
  int tick = 0;
  QPoint lastDirection{0, 0};
  int setDirectionCalls = 0;
  QList<int> selectedChoices;
  QList<ReplayFrame> inputFrames;
  QList<ChoiceRecord> choiceFrames;

  void setInternalState(int) override {
  }
  void requestStateChange(int) override {
  }
  [[nodiscard]] auto snakeModel() const -> const SnakeModel* override {
    return nullptr;
  }
  [[nodiscard]] auto headPosition() const -> QPoint override {
    return {};
  }
  void setDirection(const QPoint& direction) override {
    lastDirection = direction;
    ++setDirectionCalls;
  }
  [[nodiscard]] auto currentTick() const -> int override {
    return tick;
  }
  void recordInputAtCurrentTick(const QPoint&) override {
  }
  [[nodiscard]] auto foodPos() const -> QPoint override {
    return {};
  }
  [[nodiscard]] auto currentState() const -> int override {
    return 0;
  }
  [[nodiscard]] auto hasPendingStateChange() const -> bool override {
    return false;
  }
  [[nodiscard]] auto hasSave() const -> bool override {
    return false;
  }
  [[nodiscard]] auto hasReplay() const -> bool override {
    return !inputFrames.isEmpty();
  }
  auto advanceSessionStep(const nenoserpent::core::SessionAdvanceConfig&)
    -> nenoserpent::core::SessionAdvanceResult override {
    return {};
  }
  void restart() override {
  }
  void startReplay() override {
  }
  void loadLastSession() override {
  }
  void togglePause() override {
  }
  void nextLevel() override {
  }
  void nextPalette() override {
  }
  void startEngineTimer(int) override {
  }
  void stopEngineTimer() override {
  }
  void triggerHaptic(int) override {
  }
  void emitAudioEvent(nenoserpent::audio::Event, float) override {
  }
  void updatePersistence() override {
  }
  void advancePlayingState() override {
  }
  void enterGameOverState() override {
  }
  void enterReplayState() override {
  }
  void advanceReplayState() override {
  }
  void lazyInit() override {
  }
  void lazyInitState() override {
  }
  void forceUpdate() override {
  }
  [[nodiscard]] auto choiceIndex() const -> int override {
    return -1;
  }
  void setChoiceIndex(int) override {
  }
  [[nodiscard]] auto libraryIndex() const -> int override {
    return 0;
  }
  [[nodiscard]] auto fruitLibrarySize() const -> int override {
    return 0;
  }
  void setLibraryIndex(int) override {
  }
  [[nodiscard]] auto medalIndex() const -> int override {
    return 0;
  }
  [[nodiscard]] auto medalLibrarySize() const -> int override {
    return 0;
  }
  void setMedalIndex(int) override {
  }
  void generateChoices() override {
  }
  void selectChoice(int index) override {
    selectedChoices.append(index);
  }
};

void TestCoreRules::testCollectFreeSpotsRespectsPredicate() {
  const QList<QPoint> freeSpots =
    nenoserpent::core::collectFreeSpots(3, 2, [](const QPoint& point) -> bool {
      return point == QPoint(0, 0) || point == QPoint(2, 1);
    });

  QCOMPARE(freeSpots.size(), 4);
  QVERIFY(!freeSpots.contains(QPoint(0, 0)));
  QVERIFY(!freeSpots.contains(QPoint(2, 1)));
}

void TestCoreRules::testPickRandomFreeSpotUsesProvidedIndexAndHandlesEdgeCases() {
  QPoint picked;
  const bool ok = nenoserpent::core::pickRandomFreeSpot(
    3,
    2,
    [](const QPoint& point) -> bool { return point == QPoint(0, 0) || point == QPoint(2, 1); },
    [](int size) -> int {
      if (size != 4) {
        return -1;
      }
      return 2;
    },
    picked);
  QVERIFY(ok);
  QCOMPARE(picked, QPoint(1, 1));

  const bool badIndex = nenoserpent::core::pickRandomFreeSpot(
    2, 1, [](const QPoint&) -> bool { return false; }, [](int) -> int { return 99; }, picked);
  QVERIFY(!badIndex);

  const bool noFreeSpot = nenoserpent::core::pickRandomFreeSpot(
    2, 2, [](const QPoint&) -> bool { return true; }, [](int) -> int { return 0; }, picked);
  QVERIFY(!noFreeSpot);
}

void TestCoreRules::testMagnetCandidateSpotsPrioritizesXAxisWhenDistanceIsGreater() {
  const QList<QPoint> candidates =
    nenoserpent::core::magnetCandidateSpots(QPoint(1, 1), QPoint(9, 2), 20, 20);
  QVERIFY(!candidates.isEmpty());
  QCOMPARE(candidates.first(), QPoint(2, 1));
}

void TestCoreRules::testProbeCollisionRespectsGhostFlag() {
  const QList<QPoint> obstacles{QPoint(3, 3)};
  const std::deque<QPoint> snakeBody{QPoint(5, 5), QPoint(4, 5)};

  const nenoserpent::core::CollisionProbe obstacleHit =
    nenoserpent::core::probeCollision(QPoint(3, 3), obstacles, snakeBody, false);
  QVERIFY(obstacleHit.hitsObstacle);
  QCOMPARE(obstacleHit.obstacleIndex, 0);
  QVERIFY(!obstacleHit.hitsBody);

  const nenoserpent::core::CollisionProbe ghostBodyProbe =
    nenoserpent::core::probeCollision(QPoint(4, 5), obstacles, snakeBody, true);
  QVERIFY(!ghostBodyProbe.hitsBody);
}

void TestCoreRules::testCollisionOutcomeMatchesPortalLaserAndShieldSemantics() {
  const QList<QPoint> obstacles{QPoint(3, 3)};
  const std::deque<QPoint> snakeBody{QPoint(5, 5), QPoint(4, 5)};

  const nenoserpent::core::CollisionOutcome portalOutcome = nenoserpent::core::collisionOutcomeForHead(
    QPoint(3, 3), 20, 20, obstacles, snakeBody, false, true, false, false);
  QVERIFY(!portalOutcome.collision);
  QVERIFY(!portalOutcome.consumeLaser);
  QVERIFY(!portalOutcome.consumeShield);

  const nenoserpent::core::CollisionOutcome laserOutcome = nenoserpent::core::collisionOutcomeForHead(
    QPoint(3, 3), 20, 20, obstacles, snakeBody, false, false, true, false);
  QVERIFY(!laserOutcome.collision);
  QVERIFY(laserOutcome.consumeLaser);
  QCOMPARE(laserOutcome.obstacleIndex, 0);

  const nenoserpent::core::CollisionOutcome shieldOutcome = nenoserpent::core::collisionOutcomeForHead(
    QPoint(4, 5), 20, 20, obstacles, snakeBody, false, false, false, true);
  QVERIFY(!shieldOutcome.collision);
  QVERIFY(shieldOutcome.consumeShield);
  QVERIFY(!shieldOutcome.consumeLaser);

  const nenoserpent::core::CollisionOutcome crashOutcome = nenoserpent::core::collisionOutcomeForHead(
    QPoint(4, 5), 20, 20, obstacles, snakeBody, false, false, false, false);
  QVERIFY(crashOutcome.collision);
}

void TestCoreRules::testTickIntervalForScoreUsesSpeedFloor() {
  QCOMPARE(nenoserpent::core::tickIntervalForScore(0), 200);
  QCOMPARE(nenoserpent::core::tickIntervalForScore(25), 160);
  QCOMPARE(nenoserpent::core::tickIntervalForScore(200), 60);
}

void TestCoreRules::testPickRoguelikeChoicesIsBoundedAndDeterministic() {
  const QList<nenoserpent::core::ChoiceSpec> pickA = nenoserpent::core::pickRoguelikeChoices(1234U, 3);
  const QList<nenoserpent::core::ChoiceSpec> pickB = nenoserpent::core::pickRoguelikeChoices(1234U, 3);
  QCOMPARE(pickA.size(), 3);
  QCOMPARE(pickA.size(), pickB.size());
  for (int i = 0; i < pickA.size(); ++i) {
    QCOMPARE(pickA[i].type, pickB[i].type);
    QCOMPARE(pickA[i].name, pickB[i].name);
  }

  const QList<nenoserpent::core::ChoiceSpec> bounded = nenoserpent::core::pickRoguelikeChoices(99U, 99);
  QCOMPARE(bounded.size(), 9);
}

void TestCoreRules::testDynamicLevelFallbackProducesObstacles() {
  const auto dynamicPulse = nenoserpent::core::dynamicObstaclesForLevel(u"Dynamic Pulse", 10);
  QVERIFY(dynamicPulse.has_value());
  QVERIFY(!dynamicPulse->isEmpty());

  const auto dynamicPulseSoon = nenoserpent::core::dynamicObstaclesForLevel(u"Dynamic Pulse", 11);
  QVERIFY(dynamicPulseSoon.has_value());
  QCOMPARE(dynamicPulse.value(), dynamicPulseSoon.value());

  const auto crossfire = nenoserpent::core::dynamicObstaclesForLevel(u"Crossfire", 20);
  const auto crossfireSoon = nenoserpent::core::dynamicObstaclesForLevel(u"Crossfire", 21);
  QVERIFY(crossfire.has_value());
  QVERIFY(crossfireSoon.has_value());
  QCOMPARE(crossfire.value(), crossfireSoon.value());

  const auto shiftingBox = nenoserpent::core::dynamicObstaclesForLevel(u"Shifting Box", 28);
  const auto shiftingBoxSoon = nenoserpent::core::dynamicObstaclesForLevel(u"Shifting Box", 29);
  QVERIFY(shiftingBox.has_value());
  QVERIFY(shiftingBoxSoon.has_value());
  QCOMPARE(shiftingBox.value(), shiftingBoxSoon.value());

  const auto unknown = nenoserpent::core::dynamicObstaclesForLevel(u"Classic", 10);
  QVERIFY(!unknown.has_value());
}

void TestCoreRules::testWallsFromJsonArrayParsesCoordinates() {
  QJsonArray wallsJson;
  wallsJson.append(QJsonObject{{"x", 1}, {"y", 2}});
  wallsJson.append(QJsonObject{{"x", 8}, {"y", 9}});

  const QList<QPoint> walls = nenoserpent::core::wallsFromJsonArray(wallsJson);
  QCOMPARE(walls.size(), 2);
  QCOMPARE(walls[0], QPoint(1, 2));
  QCOMPARE(walls[1], QPoint(8, 9));
}

void TestCoreRules::testResolvedLevelDataFromJsonMapsIndexAndFields() {
  QJsonArray levels;
  levels.append(QJsonObject{{"name", "L0"}, {"script", "function onTick(t){return [];}"}});
  levels.append(QJsonObject{
    {"name", "L1"},
    {"script", ""},
    {"walls", QJsonArray{QJsonObject{{"x", 3}, {"y", 4}}, QJsonObject{{"x", 5}, {"y", 6}}}}});

  const auto resolvedScript = nenoserpent::core::resolvedLevelDataFromJson(levels, 0);
  QVERIFY(resolvedScript.has_value());
  QCOMPARE(resolvedScript->name, QString("L0"));
  QVERIFY(!resolvedScript->script.isEmpty());
  QVERIFY(resolvedScript->walls.isEmpty());

  const auto resolvedWalls = nenoserpent::core::resolvedLevelDataFromJson(levels, 3);
  QVERIFY(resolvedWalls.has_value());
  QCOMPARE(resolvedWalls->name, QString("L1"));
  QVERIFY(resolvedWalls->script.isEmpty());
  QCOMPARE(resolvedWalls->walls.size(), 2);
  QCOMPARE(resolvedWalls->walls[0], QPoint(3, 4));
  QCOMPARE(resolvedWalls->walls[1], QPoint(5, 6));

  const auto empty = nenoserpent::core::resolvedLevelDataFromJson(QJsonArray{}, 0);
  QVERIFY(!empty.has_value());
}

void TestCoreRules::testResolvedLevelDataFromJsonBytesParsesDocumentEnvelope() {
  const QJsonObject level{{"name", "BytesLevel"},
                          {"script", ""},
                          {"walls", QJsonArray{QJsonObject{{"x", 11}, {"y", 12}}}}};
  const QJsonDocument document(QJsonObject{{"levels", QJsonArray{level}}});
  const auto resolved = nenoserpent::core::resolvedLevelDataFromJsonBytes(document.toJson(), 0);
  QVERIFY(resolved.has_value());
  QCOMPARE(resolved->name, QString("BytesLevel"));
  QCOMPARE(resolved->walls.size(), 1);
  QCOMPARE(resolved->walls.first(), QPoint(11, 12));

  const auto invalid =
    nenoserpent::core::resolvedLevelDataFromJsonBytes(QByteArrayLiteral("not-json"), 0);
  QVERIFY(!invalid.has_value());
}

void TestCoreRules::testLevelCountFromJsonBytesUsesFallbackOnInvalidData() {
  const QJsonDocument valid(QJsonObject{{"levels",
                                         QJsonArray{QJsonObject{{"name", "A"}},
                                                    QJsonObject{{"name", "B"}},
                                                    QJsonObject{{"name", "C"}}}}});
  QCOMPARE(nenoserpent::core::levelCountFromJsonBytes(valid.toJson(), 6), 3);
  QCOMPARE(nenoserpent::core::levelCountFromJsonBytes(QByteArrayLiteral("x"), 6), 6);
  QCOMPARE(nenoserpent::core::levelCountFromJsonBytes(QJsonDocument(QJsonObject{}).toJson(), 6), 6);
}

void TestCoreRules::testBuffRuntimeRules() {
  QCOMPARE(nenoserpent::core::foodPointsForBuff(nenoserpent::core::BuffId::None), 1);
  QCOMPARE(nenoserpent::core::foodPointsForBuff(nenoserpent::core::BuffId::Double), 2);
  QCOMPARE(nenoserpent::core::foodPointsForBuff(nenoserpent::core::BuffId::Rich), 3);

  QCOMPARE(nenoserpent::core::buffDurationTicks(nenoserpent::core::BuffId::Rich, 40), 20);
  QCOMPARE(nenoserpent::core::buffDurationTicks(nenoserpent::core::BuffId::Ghost, 40), 40);

  QCOMPARE(nenoserpent::core::miniShrinkTargetLength(10), 5);
  QCOMPARE(nenoserpent::core::miniShrinkTargetLength(5), 3);
  QCOMPARE(nenoserpent::core::miniShrinkTargetLength(2), 3);
}

void TestCoreRules::testTickBuffCountdown() {
  int ticks = 0;
  QVERIFY(!nenoserpent::core::tickBuffCountdown(ticks));
  QCOMPARE(ticks, 0);

  ticks = 2;
  QVERIFY(!nenoserpent::core::tickBuffCountdown(ticks));
  QCOMPARE(ticks, 1);
  QVERIFY(nenoserpent::core::tickBuffCountdown(ticks));
  QCOMPARE(ticks, 0);
}

void TestCoreRules::testWeightedRandomBuffIdUsesWeightsAndFallback() {
  QCOMPARE(nenoserpent::core::weightedRandomBuffId([](int) -> int { return 0; }),
           nenoserpent::core::BuffId::Ghost);
  QCOMPARE(nenoserpent::core::weightedRandomBuffId([](int) -> int { return 22; }),
           nenoserpent::core::BuffId::Mini);
  QCOMPARE(nenoserpent::core::weightedRandomBuffId([](int) -> int { return 99; }),
           nenoserpent::core::BuffId::Ghost);
}

void TestCoreRules::testReplayTimelineAppliesOnlyOnMatchingTicks() {
  FakeReplayEngine engine;
  engine.inputFrames = {{1, 1, 0}, {3, 0, -1}, {3, -1, 0}, {6, 0, 1}};
  engine.choiceFrames = {{2, 4}, {2, 5}, {4, 1}};

  int inputIndex = 0;
  int choiceIndex = 0;

  engine.tick = 0;
  nenoserpent::core::applyReplayInputsForTick(
    engine.inputFrames, engine.tick, inputIndex, [&engine](const QPoint& direction) {
      engine.setDirection(direction);
    });
  QCOMPARE(engine.setDirectionCalls, 0);
  QCOMPARE(inputIndex, 0);

  engine.tick = 1;
  nenoserpent::core::applyReplayInputsForTick(
    engine.inputFrames, engine.tick, inputIndex, [&engine](const QPoint& direction) {
      engine.setDirection(direction);
    });
  QCOMPARE(engine.setDirectionCalls, 1);
  QCOMPARE(engine.lastDirection, QPoint(1, 0));
  QCOMPARE(inputIndex, 1);

  engine.tick = 3;
  nenoserpent::core::applyReplayInputsForTick(
    engine.inputFrames, engine.tick, inputIndex, [&engine](const QPoint& direction) {
      engine.setDirection(direction);
    });
  QCOMPARE(engine.setDirectionCalls, 3);
  QCOMPARE(engine.lastDirection, QPoint(-1, 0));
  QCOMPARE(inputIndex, 3);

  engine.tick = 7;
  nenoserpent::core::applyReplayInputsForTick(
    engine.inputFrames, engine.tick, inputIndex, [&engine](const QPoint& direction) {
      engine.setDirection(direction);
    });
  QCOMPARE(engine.setDirectionCalls, 3);
  QCOMPARE(inputIndex, 4);

  engine.tick = 2;
  nenoserpent::core::applyReplayChoiceForTick(
    engine.choiceFrames, engine.tick, choiceIndex, [&engine](const int index) {
      engine.selectChoice(index);
    });
  QCOMPARE(engine.selectedChoices.size(), 1);
  QCOMPARE(engine.selectedChoices.first(), 4);
  QCOMPARE(choiceIndex, 1);

  nenoserpent::core::applyReplayChoiceForTick(
    engine.choiceFrames, engine.tick, choiceIndex, [&engine](const int index) {
      engine.selectChoice(index);
    });
  QCOMPARE(engine.selectedChoices.size(), 2);
  QCOMPARE(engine.selectedChoices.last(), 5);
  QCOMPARE(choiceIndex, 2);

  engine.tick = 4;
  nenoserpent::core::applyReplayChoiceForTick(
    engine.choiceFrames, engine.tick, choiceIndex, [&engine](const int index) {
      engine.selectChoice(index);
    });
  QCOMPARE(engine.selectedChoices.size(), 3);
  QCOMPARE(engine.selectedChoices.last(), 1);
  QCOMPARE(choiceIndex, 3);
}

QTEST_MAIN(TestCoreRules)
// NOLINTEND(readability-convert-member-functions-to-static,readability-function-cognitive-complexity,readability-named-parameter,modernize-use-designated-initializers,bugprone-unchecked-optional-access)
#include "test_core_rules.moc"
