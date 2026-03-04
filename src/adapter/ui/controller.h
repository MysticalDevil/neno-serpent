#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

class EngineAdapter;

class UiCommandController final : public QObject {
  Q_OBJECT

public:
  explicit UiCommandController(EngineAdapter* engineAdapter, QObject* parent = nullptr);

  Q_INVOKABLE void dispatch(const QString& action) const;
  Q_INVOKABLE void requestStateChange(int state) const;
  Q_INVOKABLE void cycleBgm() const;
  Q_INVOKABLE void cycleBotMode() const;
  Q_INVOKABLE void resetBotModeDefaults() const;
  Q_INVOKABLE bool setBotParam(const QString& key, int value) const;
  Q_INVOKABLE QVariantMap botStatus() const;
  Q_INVOKABLE void seedChoicePreview(const QVariantList& types = QVariantList()) const;
  Q_INVOKABLE void seedReplayBuffPreview() const;

signals:
  void paletteChanged();
  void shellChanged();
  void achievementEarned(const QString& title);
  void eventPrompt(const QString& text);

private:
  EngineAdapter* m_engineAdapter = nullptr;
};
