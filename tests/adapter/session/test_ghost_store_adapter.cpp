#include <QDataStream>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest/QtTest>

#include "adapter/ghost/store.h"

using namespace Qt::StringLiterals;

class TestGhostStoreAdapter : public QObject {
  Q_OBJECT

private slots:
  void testSaveAndLoadRoundTrip();
  void testLoadLegacyV2WithoutChoiceHistory();
};

void TestGhostStoreAdapter::testSaveAndLoadRoundTrip() {
  QTemporaryDir temporaryDir;
  QVERIFY(temporaryDir.isValid());
  const QString filePath = nenoserpent::adapter::ghostFilePathForDirectory(temporaryDir.path());

  const nenoserpent::adapter::GhostSnapshot input{
    .recording = {QPoint(1, 2), QPoint(3, 4)},
    .randomSeed = 42U,
    .inputHistory = {{.frame = 2, .dx = 1, .dy = 0}, {.frame = 5, .dx = 0, .dy = -1}},
    .levelIndex = 3,
    .choiceHistory = {{.frame = 8, .index = 1}},
  };
  QVERIFY(nenoserpent::adapter::saveGhostSnapshotToFile(filePath, input));

  nenoserpent::adapter::GhostSnapshot output;
  QVERIFY(nenoserpent::adapter::loadGhostSnapshotFromFile(filePath, output));
  QCOMPARE(output.recording, input.recording);
  QCOMPARE(output.randomSeed, input.randomSeed);
  QCOMPARE(output.inputHistory.size(), input.inputHistory.size());
  QCOMPARE(output.inputHistory[0].frame, input.inputHistory[0].frame);
  QCOMPARE(output.levelIndex, input.levelIndex);
  QCOMPARE(output.choiceHistory.size(), input.choiceHistory.size());
  QCOMPARE(output.choiceHistory[0].index, input.choiceHistory[0].index);
}

void TestGhostStoreAdapter::testLoadLegacyV2WithoutChoiceHistory() {
  QTemporaryDir temporaryDir;
  QVERIFY(temporaryDir.isValid());
  const QString filePath = nenoserpent::adapter::ghostFilePathForDirectory(temporaryDir.path());

  QFile file(filePath);
  QVERIFY(file.open(QIODevice::WriteOnly));
  QDataStream out(&file);
  const quint32 legacyMagic = 0x534E4B02;
  const QList<QPoint> recording{QPoint(9, 9)};
  const uint randomSeed = 7U;
  const QList<ReplayFrame> inputHistory{{.frame = 3, .dx = -1, .dy = 0}};
  const int levelIndex = 2;
  out << legacyMagic << recording << randomSeed << inputHistory << levelIndex;
  file.close();

  nenoserpent::adapter::GhostSnapshot output;
  QVERIFY(nenoserpent::adapter::loadGhostSnapshotFromFile(filePath, output));
  QCOMPARE(output.recording, recording);
  QCOMPARE(output.randomSeed, randomSeed);
  QCOMPARE(output.inputHistory.size(), 1);
  QCOMPARE(output.levelIndex, levelIndex);
  QVERIFY(output.choiceHistory.isEmpty());
}

QTEST_MAIN(TestGhostStoreAdapter)
#include "test_ghost_store_adapter.moc"
