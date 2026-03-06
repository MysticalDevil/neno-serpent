#include <QtTest/QtTest>

#include "adapter/bot/backend.h"
#include "adapter/bot/runtime.h"

class BotRuntimeAdapterTest final : public QObject {
  Q_OBJECT

private slots:
  void startMenuTriggersStartWhenEnabled();
  void choiceSelectionPicksChoiceAndConfirms();
  void choiceSelectionContextBoostsLaserOnDenseObstacles();
  void choiceSelectionUsesDynamicCooldownAtHighSpeed();
  void choiceSelectionDecisionPolicyAffectsCooldown();
  void cooldownDelaysNonPlayingActions();
  void usesCustomCooldownFromStrategy();
  void usesInjectedBackendForDirectionAndChoice();
  void fallsBackToSecondaryBackendWhenPrimaryUnavailable();
  void usesFallbackWhenPrimaryMissing();
  void usesSearchFallbackWhenPrimaryMlDirectionEmpty();
  void forceCenterPushRoutesToSearchCircuit();
  void usesFallbackWhenPrimaryDirectionEmpty();
  void usesFallbackWhenPrimaryChoiceEmpty();
};

void BotRuntimeAdapterTest::startMenuTriggersStartWhenEnabled() {
  nenoserpent::adapter::bot::RuntimeInput input{};
  input.enabled = true;
  input.cooldownTicks = 0;
  input.state = AppState::StartMenu;
  const auto result = nenoserpent::adapter::bot::step(input);

  QVERIFY(result.triggerStart);
  QVERIFY(result.consumeTick);
  QCOMPARE(result.nextCooldownTicks, 4);
}

void BotRuntimeAdapterTest::choiceSelectionPicksChoiceAndConfirms() {
  const QVariantList choices = {
    QVariantMap{{"type", 3}},
    QVariantMap{{"type", 4}},
    QVariantMap{{"type", 7}},
  };

  nenoserpent::adapter::bot::RuntimeInput input{};
  input.enabled = true;
  input.cooldownTicks = 0;
  input.state = AppState::ChoiceSelection;
  input.choices = choices;
  input.currentChoiceIndex = 0;
  const auto result = nenoserpent::adapter::bot::step(input);

  QVERIFY(result.triggerStart);
  QVERIFY(result.consumeTick);
  QVERIFY(result.setChoiceIndex.has_value());
  QCOMPARE(*result.setChoiceIndex, 1);
  QCOMPARE(result.nextCooldownTicks, 2);
}

void BotRuntimeAdapterTest::choiceSelectionContextBoostsLaserOnDenseObstacles() {
  const QVariantList choices = {
    QVariantMap{{"type", 5}}, // Portal
    QVariantMap{{"type", 7}}, // Laser
    QVariantMap{{"type", 6}}, // Gold
  };

  nenoserpent::adapter::bot::RuntimeInput input{};
  input.enabled = true;
  input.cooldownTicks = 0;
  input.state = AppState::ChoiceSelection;
  input.choices = choices;
  input.currentChoiceIndex = 0;
  input.snapshot.boardWidth = 20;
  input.snapshot.boardHeight = 18;
  input.snapshot.obstacles.reserve(16);
  for (int i = 0; i < 16; ++i) {
    input.snapshot.obstacles.push_back(QPoint(i % 8, i / 8));
  }

  const auto result = nenoserpent::adapter::bot::step(input);
  QVERIFY(result.triggerStart);
  QVERIFY(result.consumeTick);
  QVERIFY(result.setChoiceIndex.has_value());
  QCOMPARE(*result.setChoiceIndex, 1);
}

void BotRuntimeAdapterTest::choiceSelectionUsesDynamicCooldownAtHighSpeed() {
  nenoserpent::adapter::bot::RuntimeInput input{};
  input.enabled = true;
  input.cooldownTicks = 0;
  input.state = AppState::ChoiceSelection;
  input.choices = {
    QVariantMap{{"type", 4}},
    QVariantMap{{"type", 8}},
    QVariantMap{{"type", 9}},
  };
  input.currentChoiceIndex = 0;
  input.snapshot.score = 200;

  const auto strategy = nenoserpent::adapter::bot::defaultStrategyConfig();
  input.strategy = &strategy;

  const auto result = nenoserpent::adapter::bot::step(input);
  QVERIFY(result.setChoiceIndex.has_value());
  QVERIFY(result.triggerStart);
  QVERIFY(result.consumeTick);
  QVERIFY(result.nextCooldownTicks > strategy.choiceCooldownTicks);
}

void BotRuntimeAdapterTest::choiceSelectionDecisionPolicyAffectsCooldown() {
  class ScopedEnv final {
  public:
    explicit ScopedEnv(const char* key)
        : m_key(key),
          m_hadValue(qEnvironmentVariableIsSet(key)),
          m_previous(m_hadValue ? qgetenv(key) : QByteArray()) {
    }
    ~ScopedEnv() {
      if (m_hadValue) {
        qputenv(m_key.constData(), m_previous);
      } else {
        qunsetenv(m_key.constData());
      }
    }

  private:
    QByteArray m_key;
    bool m_hadValue = false;
    QByteArray m_previous;
  };

  auto runWithPolicy = [](const char* policy) {
    qputenv("NENOSERPENT_BOT_DECISION_POLICY", policy);
    nenoserpent::adapter::bot::RuntimeInput input{};
    input.enabled = true;
    input.state = AppState::ChoiceSelection;
    input.choices = {
      QVariantMap{{"type", 4}},
      QVariantMap{{"type", 8}},
      QVariantMap{{"type", 9}},
    };
    input.snapshot.score = 200;
    return nenoserpent::adapter::bot::step(input).nextCooldownTicks;
  };

  const ScopedEnv scoped("NENOSERPENT_BOT_DECISION_POLICY");
  const int aggressive = runWithPolicy("aggressive");
  const int balanced = runWithPolicy("balanced");
  const int conservative = runWithPolicy("conservative");

  QVERIFY(aggressive <= balanced);
  QVERIFY(balanced <= conservative);
}

void BotRuntimeAdapterTest::cooldownDelaysNonPlayingActions() {
  nenoserpent::adapter::bot::RuntimeInput input{};
  input.enabled = true;
  input.cooldownTicks = 2;
  input.state = AppState::Paused;
  const auto result = nenoserpent::adapter::bot::step(input);

  QVERIFY(!result.triggerStart);
  QVERIFY(!result.consumeTick);
  QCOMPARE(result.nextCooldownTicks, 1);
}

void BotRuntimeAdapterTest::usesCustomCooldownFromStrategy() {
  auto strategy = nenoserpent::adapter::bot::defaultStrategyConfig();
  strategy.choiceCooldownTicks = 9;
  strategy.stateActionCooldownTicks = 7;

  nenoserpent::adapter::bot::RuntimeInput startInput{};
  startInput.enabled = true;
  startInput.state = AppState::StartMenu;
  startInput.strategy = &strategy;
  const auto startResult = nenoserpent::adapter::bot::step(startInput);
  QCOMPARE(startResult.nextCooldownTicks, 7);

  nenoserpent::adapter::bot::RuntimeInput choiceInput{};
  choiceInput.enabled = true;
  choiceInput.state = AppState::ChoiceSelection;
  choiceInput.strategy = &strategy;
  choiceInput.choices = {QVariantMap{{"type", 4}}};
  const auto choiceResult = nenoserpent::adapter::bot::step(choiceInput);
  QCOMPARE(choiceResult.nextCooldownTicks, 9);
}

void BotRuntimeAdapterTest::usesInjectedBackendForDirectionAndChoice() {
  class FakeBackend final : public nenoserpent::adapter::bot::BotBackend {
  public:
    [[nodiscard]] auto name() const -> QString override {
      return QStringLiteral("fake");
    }
    [[nodiscard]] auto decideDirection(const nenoserpent::adapter::bot::Snapshot&,
                                       const nenoserpent::adapter::bot::StrategyConfig&) const
      -> std::optional<QPoint> override {
      return QPoint(1, 0);
    }
    [[nodiscard]] auto decideChoice(const QVariantList&,
                                    const nenoserpent::adapter::bot::StrategyConfig&) const
      -> int override {
      return 2;
    }
  };

  const FakeBackend backend{};

  nenoserpent::adapter::bot::RuntimeInput playing{};
  playing.enabled = true;
  playing.state = AppState::Playing;
  playing.snapshot.body = {QPoint(10, 10), QPoint(10, 11)};
  playing.backend = &backend;
  const auto playingResult = nenoserpent::adapter::bot::step(playing);
  QVERIFY(playingResult.enqueueDirection.has_value());
  QCOMPARE(*playingResult.enqueueDirection, QPoint(1, 0));

  nenoserpent::adapter::bot::RuntimeInput choice{};
  choice.enabled = true;
  choice.state = AppState::ChoiceSelection;
  choice.choices = {
    QVariantMap{{"type", 1}},
    QVariantMap{{"type", 2}},
    QVariantMap{{"type", 3}},
  };
  choice.backend = &backend;
  const auto choiceResult = nenoserpent::adapter::bot::step(choice);
  QVERIFY(choiceResult.setChoiceIndex.has_value());
  QCOMPARE(*choiceResult.setChoiceIndex, 2);
  QVERIFY(choiceResult.triggerStart);
}

void BotRuntimeAdapterTest::fallsBackToSecondaryBackendWhenPrimaryUnavailable() {
  class UnavailableBackend final : public nenoserpent::adapter::bot::BotBackend {
  public:
    [[nodiscard]] auto name() const -> QString override {
      return QStringLiteral("ml");
    }
    [[nodiscard]] auto isAvailable() const -> bool override {
      return false;
    }
    [[nodiscard]] auto decideDirection(const nenoserpent::adapter::bot::Snapshot&,
                                       const nenoserpent::adapter::bot::StrategyConfig&) const
      -> std::optional<QPoint> override {
      return std::nullopt;
    }
    [[nodiscard]] auto decideChoice(const QVariantList&,
                                    const nenoserpent::adapter::bot::StrategyConfig&) const
      -> int override {
      return -1;
    }
  };

  class RuleLikeBackend final : public nenoserpent::adapter::bot::BotBackend {
  public:
    [[nodiscard]] auto name() const -> QString override {
      return QStringLiteral("rule");
    }
    [[nodiscard]] auto decideDirection(const nenoserpent::adapter::bot::Snapshot&,
                                       const nenoserpent::adapter::bot::StrategyConfig&) const
      -> std::optional<QPoint> override {
      return QPoint(0, -1);
    }
    [[nodiscard]] auto decideChoice(const QVariantList&,
                                    const nenoserpent::adapter::bot::StrategyConfig&) const
      -> int override {
      return 1;
    }
  };

  const UnavailableBackend primary{};
  const RuleLikeBackend fallback{};

  nenoserpent::adapter::bot::RuntimeInput input{};
  input.enabled = true;
  input.state = AppState::Playing;
  input.snapshot.body = {QPoint(10, 10), QPoint(10, 11)};
  input.backend = &primary;
  input.fallbackBackend = &fallback;

  const auto result = nenoserpent::adapter::bot::step(input);
  QVERIFY(result.enqueueDirection.has_value());
  QCOMPARE(*result.enqueueDirection, QPoint(0, -1));
  QCOMPARE(result.backend, QStringLiteral("rule"));
  QVERIFY(result.usedFallback);
  QCOMPARE(result.fallbackReason, QStringLiteral("backend-unavailable"));
}

void BotRuntimeAdapterTest::usesFallbackWhenPrimaryMissing() {
  class RuleLikeBackend final : public nenoserpent::adapter::bot::BotBackend {
  public:
    [[nodiscard]] auto name() const -> QString override {
      return QStringLiteral("rule");
    }
    [[nodiscard]] auto decideDirection(const nenoserpent::adapter::bot::Snapshot&,
                                       const nenoserpent::adapter::bot::StrategyConfig&) const
      -> std::optional<QPoint> override {
      return QPoint(-1, 0);
    }
    [[nodiscard]] auto decideChoice(const QVariantList&,
                                    const nenoserpent::adapter::bot::StrategyConfig&) const
      -> int override {
      return 0;
    }
  };

  const RuleLikeBackend fallback{};

  nenoserpent::adapter::bot::RuntimeInput input{};
  input.enabled = true;
  input.state = AppState::Playing;
  input.snapshot.body = {QPoint(8, 8), QPoint(8, 9)};
  input.backend = nullptr;
  input.fallbackBackend = &fallback;

  const auto result = nenoserpent::adapter::bot::step(input);
  QVERIFY(result.enqueueDirection.has_value());
  QCOMPARE(*result.enqueueDirection, QPoint(-1, 0));
  QCOMPARE(result.backend, QStringLiteral("rule"));
  QVERIFY(result.usedFallback);
  QCOMPARE(result.fallbackReason, QStringLiteral("primary-missing"));
}

void BotRuntimeAdapterTest::usesFallbackWhenPrimaryDirectionEmpty() {
  class EmptyPrimaryBackend final : public nenoserpent::adapter::bot::BotBackend {
  public:
    [[nodiscard]] auto name() const -> QString override {
      return QStringLiteral("ml");
    }
    [[nodiscard]] auto decideDirection(const nenoserpent::adapter::bot::Snapshot&,
                                       const nenoserpent::adapter::bot::StrategyConfig&) const
      -> std::optional<QPoint> override {
      return std::nullopt;
    }
    [[nodiscard]] auto decideChoice(const QVariantList&,
                                    const nenoserpent::adapter::bot::StrategyConfig&) const
      -> int override {
      return -1;
    }
  };

  class RuleLikeBackend final : public nenoserpent::adapter::bot::BotBackend {
  public:
    [[nodiscard]] auto name() const -> QString override {
      return QStringLiteral("rule");
    }
    [[nodiscard]] auto decideDirection(const nenoserpent::adapter::bot::Snapshot&,
                                       const nenoserpent::adapter::bot::StrategyConfig&) const
      -> std::optional<QPoint> override {
      return QPoint(0, 1);
    }
    [[nodiscard]] auto decideChoice(const QVariantList&,
                                    const nenoserpent::adapter::bot::StrategyConfig&) const
      -> int override {
      return 1;
    }
  };

  const EmptyPrimaryBackend primary{};
  const RuleLikeBackend fallback{};

  nenoserpent::adapter::bot::RuntimeInput input{};
  input.enabled = true;
  input.state = AppState::Playing;
  input.snapshot.body = {QPoint(8, 8), QPoint(8, 9)};
  input.snapshot.boardWidth = 20;
  input.snapshot.boardHeight = 18;
  input.backend = &primary;
  input.fallbackBackend = &fallback;

  const auto result = nenoserpent::adapter::bot::step(input);
  QVERIFY(result.enqueueDirection.has_value());
  QVERIFY(result.usedFallback);
  QVERIFY(result.backend == QStringLiteral("search") || result.backend == QStringLiteral("rule"));
  QVERIFY(result.fallbackReason == QStringLiteral("direction-empty-search") ||
          result.fallbackReason == QStringLiteral("direction-empty-rule"));
}

void BotRuntimeAdapterTest::usesSearchFallbackWhenPrimaryMlDirectionEmpty() {
  class EmptyPrimaryBackend final : public nenoserpent::adapter::bot::BotBackend {
  public:
    [[nodiscard]] auto name() const -> QString override {
      return QStringLiteral("ml");
    }
    [[nodiscard]] auto decideDirection(const nenoserpent::adapter::bot::Snapshot&,
                                       const nenoserpent::adapter::bot::StrategyConfig&) const
      -> std::optional<QPoint> override {
      return std::nullopt;
    }
    [[nodiscard]] auto decideChoice(const QVariantList&,
                                    const nenoserpent::adapter::bot::StrategyConfig&) const
      -> int override {
      return -1;
    }
  };

  class RuleLikeBackend final : public nenoserpent::adapter::bot::BotBackend {
  public:
    [[nodiscard]] auto name() const -> QString override {
      return QStringLiteral("rule");
    }
    [[nodiscard]] auto decideDirection(const nenoserpent::adapter::bot::Snapshot&,
                                       const nenoserpent::adapter::bot::StrategyConfig&) const
      -> std::optional<QPoint> override {
      return QPoint(0, 1);
    }
    [[nodiscard]] auto decideChoice(const QVariantList&,
                                    const nenoserpent::adapter::bot::StrategyConfig&) const
      -> int override {
      return 1;
    }
  };

  const EmptyPrimaryBackend primary{};
  const RuleLikeBackend fallback{};

  nenoserpent::adapter::bot::RuntimeInput input{};
  input.enabled = true;
  input.state = AppState::Playing;
  input.snapshot.body = {QPoint(10, 10), QPoint(10, 11), QPoint(10, 12)};
  input.snapshot.head = QPoint(10, 10);
  input.snapshot.direction = QPoint(0, -1);
  input.snapshot.food = QPoint(15, 10);
  input.snapshot.boardWidth = 20;
  input.snapshot.boardHeight = 18;
  input.backend = &primary;
  input.fallbackBackend = &fallback;

  const auto result = nenoserpent::adapter::bot::step(input);
  QVERIFY(result.enqueueDirection.has_value());
  QVERIFY(result.backend == QStringLiteral("search") || result.backend == QStringLiteral("rule"));
  QVERIFY(result.usedFallback);
  QVERIFY(result.fallbackReason == QStringLiteral("direction-empty-search") ||
          result.fallbackReason == QStringLiteral("direction-empty-rule"));
}

void BotRuntimeAdapterTest::forceCenterPushRoutesToSearchCircuit() {
  class DirectionBackend final : public nenoserpent::adapter::bot::BotBackend {
  public:
    [[nodiscard]] auto name() const -> QString override {
      return QStringLiteral("rule");
    }
    [[nodiscard]] auto decideDirection(const nenoserpent::adapter::bot::Snapshot&,
                                       const nenoserpent::adapter::bot::StrategyConfig&) const
      -> std::optional<QPoint> override {
      return QPoint(-1, 0);
    }
    [[nodiscard]] auto decideChoice(const QVariantList&,
                                    const nenoserpent::adapter::bot::StrategyConfig&) const
      -> int override {
      return 0;
    }
  };

  const DirectionBackend primary{};

  nenoserpent::adapter::bot::RuntimeInput input{};
  input.enabled = true;
  input.state = AppState::Playing;
  input.snapshot.boardWidth = 20;
  input.snapshot.boardHeight = 18;
  input.snapshot.head = QPoint(10, 10);
  input.snapshot.direction = QPoint(0, -1);
  input.snapshot.food = QPoint(0, 0);
  input.snapshot.body = {QPoint(10, 10), QPoint(10, 11), QPoint(10, 12)};
  input.backend = &primary;
  input.forceCenterPush = true;

  const auto result = nenoserpent::adapter::bot::step(input);
  QVERIFY(result.enqueueDirection.has_value());
  QCOMPARE(result.backend, QStringLiteral("search"));
  QVERIFY(result.usedFallback);
  QCOMPARE(result.fallbackReason, QStringLiteral("direction-empty-search-circuit"));
}

void BotRuntimeAdapterTest::usesFallbackWhenPrimaryChoiceEmpty() {
  class EmptyPrimaryBackend final : public nenoserpent::adapter::bot::BotBackend {
  public:
    [[nodiscard]] auto name() const -> QString override {
      return QStringLiteral("ml");
    }
    [[nodiscard]] auto decideDirection(const nenoserpent::adapter::bot::Snapshot&,
                                       const nenoserpent::adapter::bot::StrategyConfig&) const
      -> std::optional<QPoint> override {
      return std::nullopt;
    }
    [[nodiscard]] auto decideChoice(const QVariantList&,
                                    const nenoserpent::adapter::bot::StrategyConfig&) const
      -> int override {
      return -1;
    }
  };

  class RuleLikeBackend final : public nenoserpent::adapter::bot::BotBackend {
  public:
    [[nodiscard]] auto name() const -> QString override {
      return QStringLiteral("rule");
    }
    [[nodiscard]] auto decideDirection(const nenoserpent::adapter::bot::Snapshot&,
                                       const nenoserpent::adapter::bot::StrategyConfig&) const
      -> std::optional<QPoint> override {
      return QPoint(0, 1);
    }
    [[nodiscard]] auto decideChoice(const QVariantList&,
                                    const nenoserpent::adapter::bot::StrategyConfig&) const
      -> int override {
      return 2;
    }
  };

  const EmptyPrimaryBackend primary{};
  const RuleLikeBackend fallback{};

  nenoserpent::adapter::bot::RuntimeInput input{};
  input.enabled = true;
  input.state = AppState::ChoiceSelection;
  input.choices = {
    QVariantMap{{"type", 1}},
    QVariantMap{{"type", 2}},
    QVariantMap{{"type", 3}},
  };
  input.currentChoiceIndex = 0;
  input.backend = &primary;
  input.fallbackBackend = &fallback;

  const auto result = nenoserpent::adapter::bot::step(input);
  QVERIFY(result.setChoiceIndex.has_value());
  QCOMPARE(*result.setChoiceIndex, 2);
  QVERIFY(result.triggerStart);
  QCOMPARE(result.backend, QStringLiteral("rule"));
  QVERIFY(result.usedFallback);
  QCOMPARE(result.fallbackReason, QStringLiteral("choice-empty"));
}

QTEST_MAIN(BotRuntimeAdapterTest)
#include "test_bot_runtime_adapter.moc"
