#include <QtTest/QtTest>

#include "adapter/bot/config_loader.h"

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

class BotConfigLoaderAdapterTest final : public QObject {
  Q_OBJECT

private slots:
  void parsesBackendAndMlGateFromEnvironment();
};

void BotConfigLoaderAdapterTest::parsesBackendAndMlGateFromEnvironment() {
  ScopedEnv backendEnv("NENOSERPENT_BOT_BACKEND");
  ScopedEnv confEnv("NENOSERPENT_BOT_ML_MIN_CONF");
  ScopedEnv marginEnv("NENOSERPENT_BOT_ML_MIN_MARGIN");

  qputenv("NENOSERPENT_BOT_BACKEND", "search");
  qputenv("NENOSERPENT_BOT_ML_MIN_CONF", "0.77");
  qputenv("NENOSERPENT_BOT_ML_MIN_MARGIN", "1.05");

  const auto config = nenoserpent::adapter::bot::loadEnvironmentConfig();
  QVERIFY(config.backendOverrideProvided);
  QVERIFY(config.backendOverride.has_value());
  QCOMPARE(*config.backendOverride, nenoserpent::adapter::bot::BotBackendMode::Search);
  QVERIFY(config.minConfidenceValid);
  QVERIFY(config.minMarginValid);
  QCOMPARE(config.minConfidence, 0.77F);
  QCOMPARE(config.minMargin, 1.05F);
}

QTEST_MAIN(BotConfigLoaderAdapterTest)
#include "test_bot_config_loader_adapter.moc"
