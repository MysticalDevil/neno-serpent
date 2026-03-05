#include <QSignalSpy>
#include <QtTest/QtTest>

#include "adapter/engine_adapter.h"
#include "adapter/view_models/audio.h"

class TestAudioSettingsViewModel : public QObject {
  Q_OBJECT

private slots:
  static void testMirrorsVolumeProperty();
  static void testVolumeSignalTracksAdapter();
  static void testSettingVolumeRoutesThroughAdapter();
};

void TestAudioSettingsViewModel::testMirrorsVolumeProperty() {
  EngineAdapter engineAdapter;
  AudioSettingsViewModel audioSettingsViewModel(&engineAdapter);

  QCOMPARE(audioSettingsViewModel.volume(), engineAdapter.volume());
}

void TestAudioSettingsViewModel::testVolumeSignalTracksAdapter() {
  EngineAdapter engineAdapter;
  AudioSettingsViewModel audioSettingsViewModel(&engineAdapter);
  QSignalSpy volumeSpy(&audioSettingsViewModel, &AudioSettingsViewModel::volumeChanged);

  engineAdapter.setVolume(0.42F);

  QCOMPARE(volumeSpy.count(), 1);
  QCOMPARE(audioSettingsViewModel.volume(), engineAdapter.volume());
}

void TestAudioSettingsViewModel::testSettingVolumeRoutesThroughAdapter() {
  EngineAdapter engineAdapter;
  AudioSettingsViewModel audioSettingsViewModel(&engineAdapter);

  audioSettingsViewModel.setVolume(0.73F);

  QCOMPARE(engineAdapter.volume(), 0.73F);
  QCOMPARE(audioSettingsViewModel.volume(), 0.73F);
}

QTEST_MAIN(TestAudioSettingsViewModel)
#include "test_audio_settings_view_model.moc"
