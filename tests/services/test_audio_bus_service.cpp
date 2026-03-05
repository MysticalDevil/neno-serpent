#include <QFile>
#include <QScopeGuard>
#include <QTemporaryDir>
#include <QtTest/QtTest>

#include "audio/cue.h"
#include "audio/score.h"
#include "services/audio/bus.h"

class TestAudioBusService : public QObject {
  Q_OBJECT

private slots:
  void testCueTableCoversAllEvents();
  void testPausedStates();
  void testStateChangePolicy();
  void testAlternateTrackSelectionPolicy();
  void testMusicTogglePolicy();
  void testDispatchEventRoutesCallbacks();
  void testUiEventsRespectCooldownPolicy();
  void testConfirmOverridesRecentUiInteract();
  void testExternalTrackOverrideLoadsFromEnvPath();
  void testTransientEventsTriggerMusicDucking();
};

void TestAudioBusService::testCueTableCoversAllEvents() {
  const auto foodCue = nenoserpent::audio::cueForEvent(nenoserpent::audio::Event::Food);
  QVERIFY(foodCue.has_value());
  QCOMPARE(foodCue->frequencyHz, 880);
  QVERIFY(foodCue->updatesScore);

  const auto uiInteractCue = nenoserpent::audio::cueForEvent(nenoserpent::audio::Event::UiInteract);
  QVERIFY(uiInteractCue.has_value());
  QCOMPARE(uiInteractCue->kind, nenoserpent::audio::CueKind::Score);
  QCOMPARE(uiInteractCue->scoreCue, nenoserpent::audio::ScoreCueId::UiInteract);

  const auto uiInteractSteps =
    nenoserpent::audio::scoreCueSteps(nenoserpent::audio::ScoreCueId::UiInteract);
  QCOMPARE(uiInteractSteps.size(), 2U);
  QCOMPARE(uiInteractSteps.front().frequencyHz, 262);

  const auto confirmCue = nenoserpent::audio::cueForEvent(nenoserpent::audio::Event::Confirm);
  QVERIFY(confirmCue.has_value());
  QCOMPARE(confirmCue->kind, nenoserpent::audio::CueKind::Score);
  QCOMPARE(confirmCue->scoreCue, nenoserpent::audio::ScoreCueId::Confirm);

  const auto confirmSteps =
    nenoserpent::audio::scoreCueSteps(nenoserpent::audio::ScoreCueId::Confirm);
  QCOMPARE(confirmSteps.size(), 3U);
  QCOMPARE(confirmSteps.front().frequencyHz, 1046);

  const auto crashCue = nenoserpent::audio::cueForEvent(nenoserpent::audio::Event::Crash);
  QVERIFY(crashCue.has_value());
  QCOMPARE(crashCue->kind, nenoserpent::audio::CueKind::Crash);
  QCOMPARE(crashCue->durationMs, 500);

  const auto menuTrack =
    nenoserpent::audio::scoreTrackSteps(nenoserpent::audio::ScoreTrackId::MenuEmeraldDawn);
  QCOMPARE(menuTrack.size(), 32U);
  QCOMPARE(menuTrack.front().leadPitch, nenoserpent::audio::Pitch::E5);
  QCOMPARE(menuTrack.back().bassPitch, nenoserpent::audio::Pitch::C3);
  QCOMPARE(menuTrack.front().leadDuty, nenoserpent::audio::PulseDuty::Quarter);

  const auto gameplayTrack =
    nenoserpent::audio::scoreTrackSteps(nenoserpent::audio::ScoreTrackId::GameplayEmeraldDawn);
  QCOMPARE(gameplayTrack.front().leadPitch, nenoserpent::audio::Pitch::A4);
  QCOMPARE(gameplayTrack.front().leadDuty, nenoserpent::audio::PulseDuty::Half);

  const auto menuNeonTrack =
    nenoserpent::audio::scoreTrackSteps(nenoserpent::audio::ScoreTrackId::MenuNeonPulse);
  QCOMPARE(menuNeonTrack.size(), 16U);
  QCOMPARE(menuNeonTrack.front().leadPitch, nenoserpent::audio::Pitch::C5);
  QCOMPARE(menuNeonTrack.front().leadDuty, nenoserpent::audio::PulseDuty::Narrow);

  const auto gameplayNeonTrack =
    nenoserpent::audio::scoreTrackSteps(nenoserpent::audio::ScoreTrackId::GameplayNeonPulse);
  QCOMPARE(gameplayNeonTrack.front().leadPitch, nenoserpent::audio::Pitch::E4);
  QCOMPARE(gameplayNeonTrack.front().bassDuty, nenoserpent::audio::PulseDuty::Wide);

  const auto replayTrack =
    nenoserpent::audio::scoreTrackSteps(nenoserpent::audio::ScoreTrackId::ReplayEmeraldDawn);
  QCOMPARE(replayTrack.size(), 12U);
  QCOMPARE(replayTrack.front().leadPitch, nenoserpent::audio::Pitch::C5);
  QCOMPARE(replayTrack.front().bassPitch, nenoserpent::audio::Pitch::C3);

  const auto replayNeonTrack =
    nenoserpent::audio::scoreTrackSteps(nenoserpent::audio::ScoreTrackId::ReplayNeonPulse);
  QCOMPARE(replayNeonTrack.size(), 8U);
  QCOMPARE(replayNeonTrack.front().leadPitch, nenoserpent::audio::Pitch::A4);
  QCOMPARE(replayNeonTrack.front().leadDuty, nenoserpent::audio::PulseDuty::Wide);

  QCOMPARE(nenoserpent::audio::bgmVariantCount(), 4);
  QCOMPARE(nenoserpent::audio::bgmVariantName(0), u"EMERALD DAWN");
  QCOMPARE(nenoserpent::audio::bgmVariantName(1), u"NEON PULSE");
  QCOMPARE(nenoserpent::audio::bgmVariantName(2), u"CIPHER RUN");
  QCOMPARE(nenoserpent::audio::bgmVariantName(3), u"AFTERGLOW ECHO");
}

void TestAudioBusService::testPausedStates() {
  QVERIFY(!nenoserpent::services::AudioBus::pausedForState(0));
  QVERIFY(!nenoserpent::services::AudioBus::pausedForState(1));
  QVERIFY(!nenoserpent::services::AudioBus::pausedForState(2));
  QVERIFY(nenoserpent::services::AudioBus::pausedForState(3));
  QVERIFY(nenoserpent::services::AudioBus::pausedForState(6));
  QVERIFY(nenoserpent::services::AudioBus::pausedForState(7));
  QVERIFY(nenoserpent::services::AudioBus::pausedForState(8));
}

void TestAudioBusService::testStateChangePolicy() {
  int startCount = 0;
  int startedTrack = -1;
  int stopCount = 0;
  int pausedState = -1;
  int deferredDelay = 0;
  int deferredStartCount = 0;

  nenoserpent::services::AudioBus audioBus({
    .startMusic = [&](const nenoserpent::audio::ScoreTrackId trackId) -> void {
      startCount++;
      startedTrack = static_cast<int>(trackId);
      deferredStartCount++;
    },
    .stopMusic = [&]() -> void { stopCount++; },
    .setPaused = [&](const bool paused) -> void { pausedState = paused ? 1 : 0; },
  });

  audioBus.syncPausedState(3);
  QCOMPARE(pausedState, 1);

  audioBus.handleStateChanged(
    1, true, 0, [&](const int delayMs, const std::function<void()>& callback) -> void {
      deferredDelay = delayMs;
      callback();
    });
  QCOMPARE(deferredDelay, 650);
  QCOMPARE(startCount, 1);
  QCOMPARE(deferredStartCount, 1);
  QCOMPARE(startedTrack, static_cast<int>(nenoserpent::audio::ScoreTrackId::MenuEmeraldDawn));

  audioBus.handleStateChanged(2, true, 0, {});
  QCOMPARE(startCount, 2);
  QCOMPARE(startedTrack, static_cast<int>(nenoserpent::audio::ScoreTrackId::GameplayEmeraldDawn));

  audioBus.handleStateChanged(5, true, 0, {});
  QCOMPARE(startCount, 3);
  QCOMPARE(startedTrack, static_cast<int>(nenoserpent::audio::ScoreTrackId::ReplayEmeraldDawn));

  audioBus.handleStateChanged(4, true, 0, {});
  QCOMPARE(stopCount, 1);
}

void TestAudioBusService::testAlternateTrackSelectionPolicy() {
  QCOMPARE(nenoserpent::services::AudioBus::musicTrackForState(1, 0),
           nenoserpent::audio::ScoreTrackId::MenuEmeraldDawn);
  QCOMPARE(nenoserpent::services::AudioBus::musicTrackForState(1, 1),
           nenoserpent::audio::ScoreTrackId::MenuNeonPulse);
  QCOMPARE(nenoserpent::services::AudioBus::musicTrackForState(1, 2),
           nenoserpent::audio::ScoreTrackId::MenuCipherRun);
  QCOMPARE(nenoserpent::services::AudioBus::musicTrackForState(1, 3),
           nenoserpent::audio::ScoreTrackId::MenuAfterglowEcho);
  QCOMPARE(nenoserpent::services::AudioBus::musicTrackForState(2, 0),
           nenoserpent::audio::ScoreTrackId::GameplayEmeraldDawn);
  QCOMPARE(nenoserpent::services::AudioBus::musicTrackForState(2, 1),
           nenoserpent::audio::ScoreTrackId::GameplayNeonPulse);
  QCOMPARE(nenoserpent::services::AudioBus::musicTrackForState(2, 2),
           nenoserpent::audio::ScoreTrackId::GameplayCipherRun);
  QCOMPARE(nenoserpent::services::AudioBus::musicTrackForState(2, 3),
           nenoserpent::audio::ScoreTrackId::GameplayAfterglowEcho);
  QCOMPARE(nenoserpent::services::AudioBus::musicTrackForState(5, 0),
           nenoserpent::audio::ScoreTrackId::ReplayEmeraldDawn);
  QCOMPARE(nenoserpent::services::AudioBus::musicTrackForState(5, 1),
           nenoserpent::audio::ScoreTrackId::ReplayNeonPulse);
  QCOMPARE(nenoserpent::services::AudioBus::musicTrackForState(5, 2),
           nenoserpent::audio::ScoreTrackId::ReplayCipherRun);
  QCOMPARE(nenoserpent::services::AudioBus::musicTrackForState(5, 3),
           nenoserpent::audio::ScoreTrackId::ReplayAfterglowEcho);
}

void TestAudioBusService::testMusicTogglePolicy() {
  int startCount = 0;
  int stopCount = 0;
  int musicEnabled = -1;
  int startedTrack = -1;

  nenoserpent::services::AudioBus audioBus({
    .startMusic = [&](const nenoserpent::audio::ScoreTrackId trackId) -> void {
      startCount++;
      startedTrack = static_cast<int>(trackId);
    },
    .stopMusic = [&]() -> void { stopCount++; },
    .setMusicEnabled = [&](const bool enabled) -> void { musicEnabled = enabled ? 1 : 0; },
  });

  audioBus.handleMusicToggle(true, 2, 0);
  QCOMPARE(musicEnabled, 1);
  QCOMPARE(startCount, 1);
  QCOMPARE(startedTrack, static_cast<int>(nenoserpent::audio::ScoreTrackId::GameplayEmeraldDawn));
  QCOMPARE(stopCount, 0);

  audioBus.handleMusicToggle(false, 2, 0);
  QCOMPARE(musicEnabled, 0);
  QCOMPARE(stopCount, 1);
}

void TestAudioBusService::testDispatchEventRoutesCallbacks() {
  int score = -1;
  int beepFrequency = 0;
  int beepDuration = 0;
  float beepPan = 0.0F;
  int crashDuration = 0;
  int scoreCueId = -1;
  int scoreCueCount = 0;

  nenoserpent::services::AudioBus audioBus({
    .setScore = [&](const int value) -> void { score = value; },
    .playBeep = [&](const int frequencyHz, const int durationMs, const float pan) -> void {
      beepFrequency = frequencyHz;
      beepDuration = durationMs;
      beepPan = pan;
    },
    .playScoreCue = [&](const nenoserpent::audio::ScoreCueId cueId, const float) -> void {
      scoreCueId = static_cast<int>(cueId);
      scoreCueCount++;
    },
    .playCrash = [&](const int durationMs) -> void { crashDuration = durationMs; },
  });

  audioBus.dispatchEvent(nenoserpent::audio::Event::Food, {.score = 42, .pan = -0.25F});
  QCOMPARE(score, 42);
  QCOMPARE(beepFrequency, 880);
  QCOMPARE(beepDuration, 100);
  QCOMPARE(beepPan, -0.25F);

  audioBus.dispatchEvent(nenoserpent::audio::Event::PowerUp);
  QCOMPARE(beepFrequency, 1200);
  QCOMPARE(beepDuration, 150);
  QCOMPARE(beepPan, 0.0F);

  audioBus.dispatchEvent(nenoserpent::audio::Event::UiInteract);
  QCOMPARE(scoreCueId, static_cast<int>(nenoserpent::audio::ScoreCueId::UiInteract));
  QCOMPARE(scoreCueCount, 1);

  audioBus.dispatchEvent(nenoserpent::audio::Event::Confirm);
  QCOMPARE(scoreCueId, static_cast<int>(nenoserpent::audio::ScoreCueId::Confirm));
  QCOMPARE(scoreCueCount, 2);

  audioBus.dispatchEvent(nenoserpent::audio::Event::Crash);
  QCOMPARE(crashDuration, 500);
}

void TestAudioBusService::testUiEventsRespectCooldownPolicy() {
  int scoreCueCount = 0;
  int lastScoreCueId = -1;

  nenoserpent::services::AudioBus audioBus({
    .playScoreCue = [&](const nenoserpent::audio::ScoreCueId cueId, const float) -> void {
      scoreCueCount++;
      lastScoreCueId = static_cast<int>(cueId);
    },
  });

  audioBus.dispatchEvent(nenoserpent::audio::Event::UiInteract);
  audioBus.dispatchEvent(nenoserpent::audio::Event::UiInteract);

  QCOMPARE(scoreCueCount, 1);
  QCOMPARE(lastScoreCueId, static_cast<int>(nenoserpent::audio::ScoreCueId::UiInteract));
}

void TestAudioBusService::testConfirmOverridesRecentUiInteract() {
  QList<int> scoreCueIds;

  nenoserpent::services::AudioBus audioBus({
    .playScoreCue = [&](const nenoserpent::audio::ScoreCueId cueId, const float) -> void {
      scoreCueIds.push_back(static_cast<int>(cueId));
    },
  });

  audioBus.dispatchEvent(nenoserpent::audio::Event::UiInteract);
  audioBus.dispatchEvent(nenoserpent::audio::Event::Confirm);
  audioBus.dispatchEvent(nenoserpent::audio::Event::UiInteract);

  QCOMPARE(scoreCueIds.size(), 2);
  QCOMPARE(scoreCueIds.at(0), static_cast<int>(nenoserpent::audio::ScoreCueId::UiInteract));
  QCOMPARE(scoreCueIds.at(1), static_cast<int>(nenoserpent::audio::ScoreCueId::Confirm));
}

void TestAudioBusService::testExternalTrackOverrideLoadsFromEnvPath() {
  QTemporaryDir tempDir;
  QVERIFY(tempDir.isValid());

  const auto overridePath = tempDir.filePath("custom_tracks.json");
  QFile overrideFile(overridePath);
  QVERIFY(overrideFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
  overrideFile.write(R"json({
    "tracks": {
      "replay": [
        {
          "lead": "A5",
          "bass": "A3",
          "durationMs": 180,
          "leadDuty": "wide",
          "bassDuty": "quarter"
        }
      ],
      "menu_neon_pulse": [
        {
          "lead": "D5",
          "bass": "D3",
          "durationMs": 150,
          "leadDuty": "half",
          "bassDuty": "wide"
        }
      ]
    }
  })json");
  overrideFile.close();

  qputenv("NENOSERPENT_SCORE_OVERRIDE_FILE", overridePath.toUtf8());
  const auto restoreEnv = qScopeGuard([]() { qunsetenv("NENOSERPENT_SCORE_OVERRIDE_FILE"); });

  const auto replayTrack =
    nenoserpent::audio::scoreTrackSteps(nenoserpent::audio::ScoreTrackId::ReplayEmeraldDawn);
  QCOMPARE(replayTrack.size(), 1U);
  QCOMPARE(replayTrack.front().leadPitch, nenoserpent::audio::Pitch::A5);
  QCOMPARE(replayTrack.front().bassPitch, nenoserpent::audio::Pitch::A3);
  QCOMPARE(replayTrack.front().leadDuty, nenoserpent::audio::PulseDuty::Wide);
  QCOMPARE(replayTrack.front().bassDuty, nenoserpent::audio::PulseDuty::Quarter);

  const auto menuNeonTrack =
    nenoserpent::audio::scoreTrackSteps(nenoserpent::audio::ScoreTrackId::MenuNeonPulse);
  QCOMPARE(menuNeonTrack.size(), 1U);
  QCOMPARE(menuNeonTrack.front().leadPitch, nenoserpent::audio::Pitch::D5);
  QCOMPARE(menuNeonTrack.front().bassPitch, nenoserpent::audio::Pitch::D3);
}

void TestAudioBusService::testTransientEventsTriggerMusicDucking() {
  QList<QPair<float, int>> duckingCalls;

  nenoserpent::services::AudioBus audioBus({
    .duckMusic = [&](const float scale, const int durationMs) -> void {
      duckingCalls.push_back({scale, durationMs});
    },
  });

  audioBus.dispatchEvent(nenoserpent::audio::Event::UiInteract);
  audioBus.dispatchEvent(nenoserpent::audio::Event::Confirm);
  audioBus.dispatchEvent(nenoserpent::audio::Event::PowerUp);
  audioBus.dispatchEvent(nenoserpent::audio::Event::Crash);

  QCOMPARE(duckingCalls.size(), 4);
  QCOMPARE(duckingCalls.at(0).first, 0.82F);
  QCOMPARE(duckingCalls.at(0).second, 70);
  QCOMPARE(duckingCalls.at(1).first, 0.68F);
  QCOMPARE(duckingCalls.at(1).second, 110);
  QCOMPARE(duckingCalls.at(2).first, 0.72F);
  QCOMPARE(duckingCalls.at(2).second, 130);
  QCOMPARE(duckingCalls.at(3).first, 0.35F);
  QCOMPARE(duckingCalls.at(3).second, 240);
}

QTEST_MAIN(TestAudioBusService)
#include "test_audio_bus_service.moc"
