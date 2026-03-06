#include <QtTest/QtTest>

#include "adapter/bot/state.h"

namespace {

class ScopedEnv final {
public:
  explicit ScopedEnv(const QByteArray& name)
      : m_name(name),
        m_hadValue(qEnvironmentVariableIsSet(name.constData())),
        m_value(qgetenv(name.constData())) {
  }
  ScopedEnv(const ScopedEnv&) = delete;
  auto operator=(const ScopedEnv&) -> ScopedEnv& = delete;
  ScopedEnv(ScopedEnv&&) = delete;
  auto operator=(ScopedEnv&&) -> ScopedEnv& = delete;

  ~ScopedEnv() {
    if (m_hadValue) {
      qputenv(m_name.constData(), m_value);
    } else {
      qunsetenv(m_name.constData());
    }
  }

private:
  QByteArray m_name;
  bool m_hadValue = false;
  QByteArray m_value;
};

} // namespace

class BotStateAdapterTest final : public QObject {
  Q_OBJECT

private slots:
  void cycleBackendModeUpdatesAutoplayState();
  void setParamClampsAndAppearsInStatus();
  void initializeFromEnvironmentHonorsBackendOverride();
  void directionEmptySearchFuseActivatesForceCenterWindow();
  void groupedStrategyParamsAppearInStatus();
};

void BotStateAdapterTest::cycleBackendModeUpdatesAutoplayState() {
  nenoserpent::adapter::bot::State state;
  QCOMPARE(state.backendModeName(), QStringLiteral("off"));
  QVERIFY(!state.autoplayEnabled());

  state.cycleBackendMode();
  QCOMPARE(state.backendModeName(), QStringLiteral("human"));
  QVERIFY(!state.autoplayEnabled());

  state.cycleBackendMode();
  QCOMPARE(state.backendModeName(), QStringLiteral("rule"));
  QVERIFY(state.autoplayEnabled());

  state.cycleBackendMode();
  QCOMPARE(state.backendModeName(), QStringLiteral("ml"));

  state.cycleBackendMode();
  QCOMPARE(state.backendModeName(), QStringLiteral("ml-online"));
}

void BotStateAdapterTest::setParamClampsAndAppearsInStatus() {
  nenoserpent::adapter::bot::State state;
  QVERIFY(state.setParam(QStringLiteral("openSpaceWeight"), 99));
  QVERIFY(state.setParam(QStringLiteral("stateActionCooldownTicks"), -12));
  QVERIFY(!state.setParam(QStringLiteral("unknownParam"), 10));

  const auto status = state.status();
  QCOMPARE(status.value(QStringLiteral("openSpaceWeight")).toInt(), 60);
  QCOMPARE(status.value(QStringLiteral("stateActionCooldownTicks")).toInt(), 0);
}

void BotStateAdapterTest::initializeFromEnvironmentHonorsBackendOverride() {
  ScopedEnv backendEnv("NENOSERPENT_BOT_BACKEND");
  qputenv("NENOSERPENT_BOT_BACKEND", "search");

  nenoserpent::adapter::bot::State state;
  state.initializeFromEnvironment();
  QCOMPARE(state.backendModeName(), QStringLiteral("search"));
  QVERIFY(state.currentBackend() != nullptr);
  QCOMPARE(state.currentBackend()->name(), QStringLiteral("search"));
}

void BotStateAdapterTest::directionEmptySearchFuseActivatesForceCenterWindow() {
  ScopedEnv thresholdEnv("NENOSERPENT_BOT_DIRECTION_EMPTY_SEARCH_FUSE_THRESHOLD");
  ScopedEnv durationEnv("NENOSERPENT_BOT_DIRECTION_EMPTY_SEARCH_FUSE_FORCE_CENTER_TICKS");
  qputenv("NENOSERPENT_BOT_DIRECTION_EMPTY_SEARCH_FUSE_THRESHOLD", "2");
  qputenv("NENOSERPENT_BOT_DIRECTION_EMPTY_SEARCH_FUSE_FORCE_CENTER_TICKS", "5");

  nenoserpent::adapter::bot::State state;
  state.initializeFromEnvironment();
  QVERIFY(!state.forceCenterPushActive());

  state.observeDirectionFallback(true, QStringLiteral("direction-empty-search"));
  auto status = state.status();
  QCOMPARE(status.value(QStringLiteral("directionEmptySearchConsecutive")).toInt(), 1);
  QCOMPARE(status.value(QStringLiteral("directionEmptySearchForceCenterTicks")).toInt(), 0);

  state.observeDirectionFallback(true, QStringLiteral("direction-empty-search"));
  status = state.status();
  QCOMPARE(status.value(QStringLiteral("directionEmptySearchConsecutive")).toInt(), 0);
  QCOMPARE(status.value(QStringLiteral("directionEmptySearchForceCenterTicks")).toInt(), 5);
  QVERIFY(state.forceCenterPushActive());

  state.onTick();
  status = state.status();
  QCOMPARE(status.value(QStringLiteral("directionEmptySearchForceCenterTicks")).toInt(), 4);
}

void BotStateAdapterTest::groupedStrategyParamsAppearInStatus() {
  nenoserpent::adapter::bot::State state;
  QVERIFY(state.setParam(QStringLiteral("loopGuard.cycle4Penalty"), 999));
  QVERIFY(state.setParam(QStringLiteral("recovery.centerRecoverTicks"), 17));
  QVERIFY(state.setParam(QStringLiteral("modeWeights.openSpaceWeight"), 77));

  const auto status = state.status();
  QCOMPARE(status.value(QStringLiteral("modeWeights.openSpaceWeight")).toInt(), 60);
  QCOMPARE(status.value(QStringLiteral("openSpaceWeight")).toInt(), 60);
  QCOMPARE(status.value(QStringLiteral("loopGuard.cycle4Penalty")).toInt(), 600);
  QCOMPARE(status.value(QStringLiteral("recovery.centerRecoverTicks")).toInt(), 17);
  QVERIFY(status.value(QStringLiteral("deprecatedLegacyStrategyParams")).toBool());
}

QTEST_MAIN(BotStateAdapterTest)
#include "test_bot_state_adapter.moc"
