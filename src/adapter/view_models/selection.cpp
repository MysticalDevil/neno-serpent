#include "adapter/view_models/selection.h"

#include "adapter/engine.h"

SelectionViewModel::SelectionViewModel(EngineAdapter* engineAdapter, QObject* parent)
    : QObject(parent),
      m_engineAdapter(engineAdapter) {
  if (m_engineAdapter == nullptr) {
    return;
  }

  connect(
    m_engineAdapter, &EngineAdapter::choicesChanged, this, &SelectionViewModel::choicesChanged);
  connect(m_engineAdapter,
          &EngineAdapter::choicePendingChanged,
          this,
          &SelectionViewModel::choicePendingChanged);
  connect(m_engineAdapter,
          &EngineAdapter::choiceIndexChanged,
          this,
          &SelectionViewModel::choiceIndexChanged);
  connect(m_engineAdapter,
          &EngineAdapter::libraryIndexChanged,
          this,
          &SelectionViewModel::libraryIndexChanged);
  connect(m_engineAdapter,
          &EngineAdapter::fruitLibraryChanged,
          this,
          &SelectionViewModel::fruitLibraryChanged);
  connect(m_engineAdapter,
          &EngineAdapter::medalIndexChanged,
          this,
          &SelectionViewModel::medalIndexChanged);
  connect(m_engineAdapter,
          &EngineAdapter::achievementsChanged,
          this,
          &SelectionViewModel::achievementsChanged);
}

auto SelectionViewModel::choices() const -> QVariantList {
  return m_engineAdapter != nullptr ? m_engineAdapter->choices() : QVariantList{};
}

auto SelectionViewModel::choicePending() const -> bool {
  return m_engineAdapter != nullptr ? m_engineAdapter->choicePending() : false;
}

auto SelectionViewModel::choiceIndex() const -> int {
  return m_engineAdapter != nullptr ? m_engineAdapter->choiceIndex() : 0;
}

auto SelectionViewModel::fruitLibrary() const -> QVariantList {
  return m_engineAdapter != nullptr ? m_engineAdapter->fruitLibrary() : QVariantList{};
}

auto SelectionViewModel::libraryIndex() const -> int {
  return m_engineAdapter != nullptr ? m_engineAdapter->libraryIndex() : 0;
}

auto SelectionViewModel::medalLibrary() const -> QVariantList {
  return m_engineAdapter != nullptr ? m_engineAdapter->medalLibrary() : QVariantList{};
}

auto SelectionViewModel::medalIndex() const -> int {
  return m_engineAdapter != nullptr ? m_engineAdapter->medalIndex() : 0;
}

auto SelectionViewModel::achievements() const -> QVariantList {
  return m_engineAdapter != nullptr ? m_engineAdapter->achievements() : QVariantList{};
}
