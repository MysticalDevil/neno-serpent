#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest/QtTest>

#include "services/level/repository.h"

class TestLevelRepositoryService : public QObject {
  Q_OBJECT

private slots:
  void testReadLevelCount();
  void testLoadResolvedLevel();
};

void TestLevelRepositoryService::testReadLevelCount() {
  QTemporaryDir tmpDir;
  QVERIFY(tmpDir.isValid());

  const QString filePath = QDir(tmpDir.path()).filePath("levels.json");
  QFile file(filePath);
  QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
  const QJsonDocument doc(QJsonObject{
    {"levels", QJsonArray{QJsonObject{{"name", "A"}}, QJsonObject{{"name", "B"}}}},
  });
  QVERIFY(file.write(doc.toJson()) > 0);
  file.close();

  const nenoserpent::services::LevelRepository repository(filePath, 6);
  QCOMPARE(repository.levelCount(), 2);

  const nenoserpent::services::LevelRepository missingRepository(QStringLiteral("/path/not/found.json"),
                                                             6);
  QCOMPARE(missingRepository.levelCount(), 6);
}

void TestLevelRepositoryService::testLoadResolvedLevel() {
  QTemporaryDir tmpDir;
  QVERIFY(tmpDir.isValid());

  const QString filePath = QDir(tmpDir.path()).filePath("levels.json");
  QFile file(filePath);
  QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
  const QJsonDocument doc(QJsonObject{
    {"levels",
     QJsonArray{QJsonObject{{"name", "Scripted"}, {"script", "function onTick(t){return [];}"}},
                QJsonObject{{"name", "Static"},
                            {"script", ""},
                            {"walls", QJsonArray{QJsonObject{{"x", 7}, {"y", 8}}}}}}},
  });
  QVERIFY(file.write(doc.toJson()) > 0);
  file.close();

  const nenoserpent::services::LevelRepository repository(filePath, 6);
  const auto scripted = repository.loadResolvedLevel(0);
  QVERIFY(scripted.has_value());
  QCOMPARE(scripted->name, QString("Scripted"));
  QVERIFY(!scripted->script.isEmpty());
  QVERIFY(scripted->walls.isEmpty());

  const auto wrapped = repository.loadResolvedLevel(3);
  QVERIFY(wrapped.has_value());
  QCOMPARE(wrapped->name, QString("Static"));
  QVERIFY(wrapped->script.isEmpty());
  QCOMPARE(wrapped->walls.size(), 1);
  QCOMPARE(wrapped->walls.first(), QPoint(7, 8));

  const nenoserpent::services::LevelRepository missingRepository(QStringLiteral("/path/not/found.json"),
                                                             6);
  QVERIFY(!missingRepository.loadResolvedLevel(0).has_value());
}

QTEST_MAIN(TestLevelRepositoryService)
#include "test_level_repository_service.moc"
