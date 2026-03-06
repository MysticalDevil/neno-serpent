#pragma once

#include <QObject>

#include "core/buff/runtime.h"

class PowerUpId final : public QObject {
  Q_OBJECT

public:
  explicit PowerUpId(QObject* parent = nullptr)
      : QObject(parent) {
  }

  enum Value {
    None = static_cast<int>(nenoserpent::core::BuffId::None),
    Ghost = static_cast<int>(nenoserpent::core::BuffId::Ghost),
    Slow = static_cast<int>(nenoserpent::core::BuffId::Slow),
    Magnet = static_cast<int>(nenoserpent::core::BuffId::Magnet),
    Shield = static_cast<int>(nenoserpent::core::BuffId::Shield),
    Portal = static_cast<int>(nenoserpent::core::BuffId::Portal),
    Gold = static_cast<int>(nenoserpent::core::BuffId::Gold),
    Laser = static_cast<int>(nenoserpent::core::BuffId::Laser),
    Mini = static_cast<int>(nenoserpent::core::BuffId::Mini),
    Freeze = static_cast<int>(nenoserpent::core::BuffId::Freeze),
    Scout = static_cast<int>(nenoserpent::core::BuffId::Scout),
    Vacuum = static_cast<int>(nenoserpent::core::BuffId::Vacuum),
    Anchor = static_cast<int>(nenoserpent::core::BuffId::Anchor)
  };
  Q_ENUM(Value)
};
