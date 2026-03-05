#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

class EngineAdapter;
namespace nenoserpent::adapter::bot {
class BotControlPort;
}

class UiCommandController final : public QObject {
  Q_OBJECT

public:
  explicit UiCommandController(EngineAdapter* engineAdapter, QObject* parent = nullptr);

  Q_INVOKABLE void dispatch(const QString& action) const;
  Q_INVOKABLE void requestStateChange(int state) const;
  Q_INVOKABLE void cycleBgm() const;
  Q_INVOKABLE void cycleBotMode();
  Q_INVOKABLE void cycleBotStrategyMode();
  Q_INVOKABLE void resetBotModeDefaults();
  Q_INVOKABLE bool setBotParam(const QString& key, int value);
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
  nenoserpent::adapter::bot::BotControlPort* m_botControlPort = nullptr;
};
