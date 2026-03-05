#include <QSignalSpy>
#include <QtTest/QtTest>

#include "adapter/engine_adapter.h"
#include "adapter/view_models/theme.h"

class TestThemeViewModel : public QObject {
  Q_OBJECT

private slots:
  static void testMirrorsPaletteProperties();
  static void testPaletteSignalTracksAdapter();
  static void testShellSignalTracksAdapter();
};

void TestThemeViewModel::testMirrorsPaletteProperties() {
  EngineAdapter engineAdapter;
  ThemeViewModel themeViewModel(&engineAdapter);

  QCOMPARE(themeViewModel.palette(), engineAdapter.palette());
  QCOMPARE(themeViewModel.paletteName(), engineAdapter.paletteName());
  QCOMPARE(themeViewModel.shellColor(), engineAdapter.shellColor());
  QCOMPARE(themeViewModel.shellName(), engineAdapter.shellName());
}

void TestThemeViewModel::testPaletteSignalTracksAdapter() {
  EngineAdapter engineAdapter;
  ThemeViewModel themeViewModel(&engineAdapter);
  QSignalSpy paletteSpy(&themeViewModel, &ThemeViewModel::paletteChanged);

  const auto before = themeViewModel.paletteName();
  engineAdapter.nextPalette();

  QCOMPARE(paletteSpy.count(), 1);
  QVERIFY(themeViewModel.paletteName() != before);
}

void TestThemeViewModel::testShellSignalTracksAdapter() {
  EngineAdapter engineAdapter;
  ThemeViewModel themeViewModel(&engineAdapter);
  QSignalSpy shellSpy(&themeViewModel, &ThemeViewModel::shellChanged);

  const auto before = themeViewModel.shellName();
  engineAdapter.nextShellColor();

  QCOMPARE(shellSpy.count(), 1);
  QVERIFY(themeViewModel.shellName() != before);
}

QTEST_MAIN(TestThemeViewModel)
#include "test_theme_view_model.moc"
