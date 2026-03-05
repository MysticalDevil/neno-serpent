#include <QtTest/QtTest>

#include "adapter/models/choice.h"

using namespace Qt::StringLiterals;

class TestChoiceModelsAdapter : public QObject {
  Q_OBJECT

private slots:
  void testBuildChoiceModelMapsAllFields();
  void testChoiceTypeAtValidAndInvalidRows();
};

void TestChoiceModelsAdapter::testBuildChoiceModelMapsAllFields() {
  const QList<nenoserpent::core::ChoiceSpec> choices = {
    {.type = 2, .name = u"Slow"_s, .description = u"Slows game speed"_s},
    {.type = 8, .name = u"Laser"_s, .description = u"Break one wall on hit"_s},
  };

  const QVariantList model = nenoserpent::adapter::buildChoiceModel(choices);
  QCOMPARE(model.size(), 2);

  const QVariantMap first = model[0].toMap();
  QCOMPARE(first.value(u"type"_s).toInt(), 2);
  QCOMPARE(first.value(u"name"_s).toString(), QStringLiteral("Slow"));
  QCOMPARE(first.value(u"desc"_s).toString(), QStringLiteral("Slows game speed"));
}

void TestChoiceModelsAdapter::testChoiceTypeAtValidAndInvalidRows() {
  QVariantList model;
  QVariantMap valid;
  valid.insert(u"type"_s, 6);
  model.append(valid);
  model.append(QVariantMap{{u"name"_s, u"bad"_s}});

  const auto validType = nenoserpent::adapter::choiceTypeAt(model, 0);
  QVERIFY(validType.has_value());
  QCOMPARE(validType.value(), 6);

  const auto invalidShape = nenoserpent::adapter::choiceTypeAt(model, 1);
  QVERIFY(!invalidShape.has_value());

  const auto invalidIndex = nenoserpent::adapter::choiceTypeAt(model, 8);
  QVERIFY(!invalidIndex.has_value());
}

QTEST_MAIN(TestChoiceModelsAdapter)
#include "test_choice_models_adapter.moc"
