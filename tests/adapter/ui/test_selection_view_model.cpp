#include <QSignalSpy>
#include <QtTest/QtTest>

#include "adapter/engine_adapter.h"
#include "adapter/view_models/selection.h"

class TestSelectionViewModel : public QObject {
  Q_OBJECT

private slots:
  static void testMirrorsSelectionProperties();
  static void testChoicesSignalTracksAdapter();
  static void testChoiceIndexSignalTracksAdapter();
  static void testLibrarySignalsTrackAdapter();
  static void testMedalSignalsTrackAdapter();
};

void TestSelectionViewModel::testMirrorsSelectionProperties() {
  EngineAdapter engineAdapter;
  SelectionViewModel selectionViewModel(&engineAdapter);

  QCOMPARE(selectionViewModel.choices(), engineAdapter.choices());
  QCOMPARE(selectionViewModel.choicePending(), engineAdapter.choicePending());
  QCOMPARE(selectionViewModel.choiceIndex(), engineAdapter.choiceIndex());
  QCOMPARE(selectionViewModel.fruitLibrary(), engineAdapter.fruitLibrary());
  QCOMPARE(selectionViewModel.libraryIndex(), engineAdapter.libraryIndex());
  QCOMPARE(selectionViewModel.medalLibrary(), engineAdapter.medalLibrary());
  QCOMPARE(selectionViewModel.medalIndex(), engineAdapter.medalIndex());
  QCOMPARE(selectionViewModel.achievements(), engineAdapter.achievements());
}

void TestSelectionViewModel::testChoicesSignalTracksAdapter() {
  EngineAdapter engineAdapter;
  SelectionViewModel selectionViewModel(&engineAdapter);
  QSignalSpy choicesSpy(&selectionViewModel, &SelectionViewModel::choicesChanged);

  engineAdapter.generateChoices();

  QCOMPARE(choicesSpy.count(), 1);
  QCOMPARE(selectionViewModel.choices(), engineAdapter.choices());
}

void TestSelectionViewModel::testChoiceIndexSignalTracksAdapter() {
  EngineAdapter engineAdapter;
  SelectionViewModel selectionViewModel(&engineAdapter);
  QSignalSpy choiceIndexSpy(&selectionViewModel, &SelectionViewModel::choiceIndexChanged);

  engineAdapter.setChoiceIndex(1);

  QCOMPARE(choiceIndexSpy.count(), 1);
  QCOMPARE(selectionViewModel.choiceIndex(), engineAdapter.choiceIndex());
}

void TestSelectionViewModel::testLibrarySignalsTrackAdapter() {
  EngineAdapter engineAdapter;
  SelectionViewModel selectionViewModel(&engineAdapter);
  QSignalSpy librarySpy(&selectionViewModel, &SelectionViewModel::libraryIndexChanged);

  engineAdapter.setLibraryIndex(1);

  QCOMPARE(librarySpy.count(), 1);
  QCOMPARE(selectionViewModel.libraryIndex(), 1);
}

void TestSelectionViewModel::testMedalSignalsTrackAdapter() {
  EngineAdapter engineAdapter;
  SelectionViewModel selectionViewModel(&engineAdapter);
  QSignalSpy medalSpy(&selectionViewModel, &SelectionViewModel::medalIndexChanged);

  engineAdapter.setMedalIndex(1);

  QCOMPARE(medalSpy.count(), 1);
  QCOMPARE(selectionViewModel.medalIndex(), 1);
}

QTEST_MAIN(TestSelectionViewModel)
#include "test_selection_view_model.moc"
