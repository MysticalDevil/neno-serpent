#include <QtTest/QtTest>

#include "adapter/models/library.h"

using namespace Qt::StringLiterals;

class TestLibraryModelsAdapter : public QObject {
  Q_OBJECT

private slots:
  void testBuildFruitLibraryModelMasksUndiscoveredEntries();
  void testBuildMedalLibraryModelIncludesKnownRows();
};

void TestLibraryModelsAdapter::testBuildFruitLibraryModelMasksUndiscoveredEntries() {
  const QVariantList model = nenoserpent::adapter::buildFruitLibraryModel(QList<int>{1, 3});
  QCOMPARE(model.size(), 9);

  const QVariantMap ghost = model[0].toMap();
  QCOMPARE(ghost.value(u"name"_s).toString(), QStringLiteral("Ghost"));
  QVERIFY(ghost.value(u"discovered"_s).toBool());

  const QVariantMap slow = model[1].toMap();
  QCOMPARE(slow.value(u"name"_s).toString(), QStringLiteral("??????"));
  QVERIFY(!slow.value(u"discovered"_s).toBool());
}

void TestLibraryModelsAdapter::testBuildMedalLibraryModelIncludesKnownRows() {
  const QVariantList model = nenoserpent::adapter::buildMedalLibraryModel();
  QCOMPARE(model.size(), 7);
  const QVariantMap first = model[0].toMap();
  QCOMPARE(first.value(u"id"_s).toString(), QStringLiteral("Gold Medal (50 Pts)"));
  QCOMPARE(first.value(u"hint"_s).toString(), QStringLiteral("Reach 50 points"));
}

QTEST_MAIN(TestLibraryModelsAdapter)
#include "test_library_models_adapter.moc"
