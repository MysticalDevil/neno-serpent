#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest/QtTest>

#include "adapter/level/loader.h"

class TestLevelLoaderAdapter : public QObject {
  Q_OBJECT

private slots:
  void testReadLevelCountFromResource();
  void testLoadResolvedLevelFromResource();
};

void TestLevelLoaderAdapter::testReadLevelCountFromResource() {
  QTemporaryDir tmpDir;
  QVERIFY(tmpDir.isValid());

  const QString filePath = QDir(tmpDir.path()).filePath("levels.json");
  QFile file(filePath);
  QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
  const QJsonDocument doc(
    QJsonObject{{"levels", QJsonArray{QJsonObject{{"name", "A"}}, QJsonObject{{"name", "B"}}}}});
  QVERIFY(file.write(doc.toJson()) > 0);
  file.close();

  QCOMPARE(nenoserpent::adapter::readLevelCountFromResource(filePath, 6), 2);
  QCOMPARE(nenoserpent::adapter::readLevelCountFromResource(QStringLiteral("/path/not/found.json"), 6),
           6);
}

void TestLevelLoaderAdapter::testLoadResolvedLevelFromResource() {
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
                            {"walls", QJsonArray{QJsonObject{{"x", 7}, {"y", 8}}}}}}}});
  QVERIFY(file.write(doc.toJson()) > 0);
  file.close();

  const auto scripted = nenoserpent::adapter::loadResolvedLevelFromResource(filePath, 0);
  QVERIFY(scripted.has_value());
  QCOMPARE(scripted->name, QString("Scripted"));
  QVERIFY(!scripted->script.isEmpty());
  QVERIFY(scripted->walls.isEmpty());

  const auto wrapped = nenoserpent::adapter::loadResolvedLevelFromResource(filePath, 3);
  QVERIFY(wrapped.has_value());
  QCOMPARE(wrapped->name, QString("Static"));
  QVERIFY(wrapped->script.isEmpty());
  QCOMPARE(wrapped->walls.size(), 1);
  QCOMPARE(wrapped->walls.first(), QPoint(7, 8));

  const auto missing =
    nenoserpent::adapter::loadResolvedLevelFromResource(QStringLiteral("/path/not/found.json"), 0);
  QVERIFY(!missing.has_value());
}

QTEST_MAIN(TestLevelLoaderAdapter)
#include "test_level_loader_adapter.moc"
