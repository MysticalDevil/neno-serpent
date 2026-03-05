#include "states.h"

// --- Splash State ---
void SplashState::enter() {
  m_context.setInternalState(AppState::Splash);
  m_frames = 0;
  m_context.lazyInit();
  m_context.startEngineTimer(16); // ~60fps logic
}

void SplashState::update() {
  m_frames++;
  if (m_frames > 110) {
    m_context.emitAudioEvent(nenoserpent::audio::Event::Confirm);
    m_context.requestStateChange(AppState::StartMenu);
  }
}

// --- Menu State ---
void MenuState::enter() {
  m_context.setInternalState(AppState::StartMenu);
  if (m_context.botAutoplayEnabled()) {
    // Bot autoplay needs periodic ticks in menu to trigger start/resume actions.
    m_context.startEngineTimer(120);
  } else {
    m_context.stopEngineTimer();
  }
}

void MenuState::handleStart() {
  if (m_context.hasSave()) {
    m_context.loadLastSession();
  } else {
    m_context.restart();
  }
}

void MenuState::handleSelect() {
  m_context.nextLevel();
}

void MenuState::handleInput(int dx, int dy) {
  if (dy < 0) {
    m_context.requestStateChange(AppState::MedalRoom);
  } else if (dy > 0) {
    if (m_context.hasReplay()) {
      m_context.startReplay();
    } else {
      m_context.emitAudioEvent(nenoserpent::audio::Event::Confirm);
      m_context.triggerHaptic(2);
    }
  } else if (dx < 0) {
    m_context.requestStateChange(AppState::Library);
  } else if (dx > 0) {
    m_context.nextPalette();
  }
}

// --- Playing State ---
void PlayingState::enter() {
  m_context.setInternalState(AppState::Playing);
}

void PlayingState::update() {
  m_context.advancePlayingState();
}

void PlayingState::handleInput(int /*dx*/, int /*dy*/) {
  // Movement handled via queue in EngineAdapter
}

void PlayingState::handleStart() {
  m_context.requestStateChange(AppState::Paused);
}

// --- Paused State ---
void PausedState::enter() {
  m_context.setInternalState(AppState::Paused);
}

void PausedState::handleStart() {
  m_context.requestStateChange(AppState::Playing);
}

void PausedState::handleSelect() {
  m_context.requestStateChange(AppState::StartMenu);
}

// --- GameOver State ---
void GameOverState::enter() {
  m_context.enterGameOverState();
}

void GameOverState::handleStart() {
  m_context.restart();
}

void GameOverState::handleSelect() {
  m_context.requestStateChange(AppState::StartMenu);
}

// --- Choice State ---
void ChoiceState::enter() {
  m_context.setInternalState(AppState::ChoiceSelection);
  m_context.generateChoices();
}

void ChoiceState::handleInput(int /*dx*/, int dy) {
  if (dy > 0) {
    int next = (m_context.choiceIndex() + 1) % 3;
    m_context.setChoiceIndex(next);
  } else if (dy < 0) {
    int prev = (m_context.choiceIndex() + 2) % 3;
    m_context.setChoiceIndex(prev);
  }
}

void ChoiceState::handleStart() {
  m_context.selectChoice(m_context.choiceIndex());
}

void ChoiceState::handleSelect() {
  m_context.requestStateChange(AppState::StartMenu);
}

// --- Replaying State ---
void ReplayingState::enter() {
  m_context.enterReplayState();
}

void ReplayingState::update() {
  m_context.advanceReplayState();
}

void ReplayingState::handleStart() {
  m_context.requestStateChange(AppState::StartMenu);
}

void ReplayingState::handleSelect() {
  m_context.requestStateChange(AppState::StartMenu);
}

// --- Library & Medal Room ---
void LibraryState::enter() {
  m_context.setInternalState(AppState::Library);
}

void LibraryState::handleInput(int /*dx*/, int dy) {
  const int count = m_context.fruitLibrarySize();
  if (count <= 0 || dy == 0) {
    return;
  }
  const int step = (dy > 0) ? 1 : -1;
  const int next = (m_context.libraryIndex() + step + count) % count;
  m_context.setLibraryIndex(next);
}

void LibraryState::handleSelect() {
  m_context.requestStateChange(AppState::StartMenu);
}

void MedalRoomState::enter() {
  m_context.setInternalState(AppState::MedalRoom);
}

void MedalRoomState::handleInput(int /*dx*/, int dy) {
  const int count = m_context.medalLibrarySize();
  if (count <= 0 || dy == 0) {
    return;
  }
  const int step = (dy > 0) ? 1 : -1;
  const int next = (m_context.medalIndex() + step + count) % count;
  m_context.setMedalIndex(next);
}

void MedalRoomState::handleSelect() {
  m_context.requestStateChange(AppState::StartMenu);
}
