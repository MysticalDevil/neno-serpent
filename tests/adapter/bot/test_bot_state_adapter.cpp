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
};

void BotStateAdapterTest::cycleBackendModeUpdatesAutoplayState() {
  nenoserpent::adapter::bot::State state;
  QCOMPARE(state.backendModeName(), QStringLiteral("off"));
  QVERIFY(!state.autoplayEnabled());

  state.cycleBackendMode();
  QCOMPARE(state.backendModeName(), QStringLiteral("rule"));
  QVERIFY(state.autoplayEnabled());

  state.cycleBackendMode();
  QCOMPARE(state.backendModeName(), QStringLiteral("ml"));
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

QTEST_MAIN(BotStateAdapterTest)
#include "test_bot_state_adapter.moc"
