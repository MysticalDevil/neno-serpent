#include "adapter/view_models/theme.h"

#include "adapter/engine.h"

ThemeViewModel::ThemeViewModel(EngineAdapter* engineAdapter, QObject* parent)
    : QObject(parent),
      m_engineAdapter(engineAdapter) {
  if (m_engineAdapter == nullptr) {
    return;
  }

  connect(m_engineAdapter, &EngineAdapter::paletteChanged, this, &ThemeViewModel::paletteChanged);
  connect(m_engineAdapter, &EngineAdapter::shellColorChanged, this, &ThemeViewModel::shellChanged);
}

auto ThemeViewModel::palette() const -> QVariantList {
  return m_engineAdapter != nullptr ? m_engineAdapter->palette() : QVariantList{};
}

auto ThemeViewModel::paletteName() const -> QString {
  return m_engineAdapter != nullptr ? m_engineAdapter->paletteName() : QString{};
}

auto ThemeViewModel::shellColor() const -> QColor {
  return m_engineAdapter != nullptr ? m_engineAdapter->shellColor() : QColor{};
}

auto ThemeViewModel::shellName() const -> QString {
  return m_engineAdapter != nullptr ? m_engineAdapter->shellName() : QString{};
}
