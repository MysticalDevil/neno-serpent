#include <QtTest/QtTest>

#include "adapter/session/state.h"

using namespace Qt::StringLiterals;

// QtTest slot-based tests intentionally stay as member functions and use assertion-heavy bodies.
// NOLINTBEGIN(readability-convert-member-functions-to-static)
class TestSessionStateAdapter : public QObject {
  Q_OBJECT

private slots:
  void testDecodeSessionSnapshotFromVariantMap();
  void testDecodeSessionSnapshotRejectsEmptyBody();
  void testConvertSessionSnapshotToCoreStateSnapshot();
  void testConvertCoreStateSnapshotToSessionSnapshot();
};

void TestSessionStateAdapter::testDecodeSessionSnapshotFromVariantMap() {
  QVariantMap data;
  data.insert(u"score"_s, 12);
  data.insert(u"food"_s, QPoint(3, 4));
  data.insert(u"dir"_s, QPoint(0, -1));
  data.insert(u"obstacles"_s, QVariantList{QPoint(1, 1), QPoint(2, 2)});
  data.insert(u"body"_s, QVariantList{QPoint(5, 5), QPoint(5, 6)});

  const auto snapshot = nenoserpent::adapter::decodeSessionSnapshot(data);
  QVERIFY(snapshot.has_value());
  const auto decoded = snapshot.value_or(nenoserpent::adapter::SessionSnapshot{});
  QCOMPARE(decoded.score, 12);
  QCOMPARE(decoded.food, QPoint(3, 4));
  QCOMPARE(decoded.direction, QPoint(0, -1));
  QCOMPARE(decoded.obstacles.size(), 2);
  QCOMPARE(decoded.body.size(), 2);
}

void TestSessionStateAdapter::testDecodeSessionSnapshotRejectsEmptyBody() {
  QVariantMap data;
  data.insert(u"score"_s, 1);
  data.insert(u"food"_s, QPoint(0, 0));
  data.insert(u"dir"_s, QPoint(1, 0));
  data.insert(u"body"_s, QVariantList{});

  const auto snapshot = nenoserpent::adapter::decodeSessionSnapshot(data);
  QVERIFY(!snapshot.has_value());
}

void TestSessionStateAdapter::testConvertSessionSnapshotToCoreStateSnapshot() {
  const nenoserpent::adapter::SessionSnapshot snapshot{
    .score = 12,
    .food = QPoint(3, 4),
    .direction = QPoint(0, -1),
    .obstacles = {QPoint(1, 1), QPoint(2, 2)},
    .body = {QPoint(5, 5), QPoint(5, 6)},
  };

  const auto coreSnapshot = nenoserpent::adapter::toCoreStateSnapshot(snapshot);
  QCOMPARE(coreSnapshot.state.score, 12);
  QCOMPARE(coreSnapshot.state.food, QPoint(3, 4));
  QCOMPARE(coreSnapshot.state.direction, QPoint(0, -1));
  QCOMPARE(coreSnapshot.state.obstacles, QList<QPoint>({QPoint(1, 1), QPoint(2, 2)}));
  QCOMPARE(coreSnapshot.body, snapshot.body);
}

void TestSessionStateAdapter::testConvertCoreStateSnapshotToSessionSnapshot() {
  const nenoserpent::core::StateSnapshot snapshot{
    .state =
      {
        .food = QPoint(4, 6),
        .direction = QPoint(1, 0),
        .score = 27,
        .obstacles = {QPoint(3, 3), QPoint(4, 3)},
      },
    .body = {QPoint(6, 6), QPoint(5, 6), QPoint(4, 6)},
  };

  const auto persisted = nenoserpent::adapter::fromCoreStateSnapshot(snapshot);
  QCOMPARE(persisted.score, 27);
  QCOMPARE(persisted.food, QPoint(4, 6));
  QCOMPARE(persisted.direction, QPoint(1, 0));
  QCOMPARE(persisted.obstacles, QList<QPoint>({QPoint(3, 3), QPoint(4, 3)}));
  QCOMPARE(persisted.body, snapshot.body);
}

// NOLINTEND(readability-convert-member-functions-to-static)
QTEST_MAIN(TestSessionStateAdapter)
#include "test_session_state_adapter.moc"
