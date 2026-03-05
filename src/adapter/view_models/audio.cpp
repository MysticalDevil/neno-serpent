#include "adapter/view_models/audio.h"

#include "adapter/engine.h"

AudioSettingsViewModel::AudioSettingsViewModel(EngineAdapter* engineAdapter, QObject* parent)
    : QObject(parent),
      m_engineAdapter(engineAdapter) {
  if (m_engineAdapter == nullptr) {
    return;
  }

  connect(
    m_engineAdapter, &EngineAdapter::volumeChanged, this, &AudioSettingsViewModel::volumeChanged);
}

auto AudioSettingsViewModel::volume() const -> float {
  return m_engineAdapter != nullptr ? m_engineAdapter->volume() : 1.0F;
}

void AudioSettingsViewModel::setVolume(const float value) {
  if (m_engineAdapter == nullptr) {
    return;
  }
  m_engineAdapter->setVolume(value);
}
