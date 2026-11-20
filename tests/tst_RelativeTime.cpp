#include <QtTest>

#include "RelativeTime.h"

using planova::s5::formatRelative;

class TstRelativeTime : public QObject {
    Q_OBJECT
private slots:
    void formats_data()
    {
        QTest::addColumn<qint64>("diffMs");
        QTest::addColumn<QString>("expected");
        constexpr qint64 s = 1000, m = 60 * s, h = 60 * m, d = 24 * h;
        QTest::newRow("0 s") << qint64(0) << "Just now";
        QTest::newRow("59 s") << 59 * s << "Just now";
        QTest::newRow("60 s") << 60 * s << "1m ago";
        QTest::newRow("59 min") << 59 * m << "59m ago";
        QTest::newRow("60 min") << 60 * m << "1h ago";
        QTest::newRow("23 h") << 23 * h << "23h ago";
        QTest::newRow("24 h") << 24 * h << "Yesterday";
        QTest::newRow("47 h") << 47 * h << "Yesterday";
        QTest::newRow("48 h") << 48 * h << "2d ago";
        QTest::newRow("5 d") << 5 * d << "5d ago";
        QTest::newRow("future") << -5 * m << "Just now";
    }
    void formats()
    {
        QFETCH(qint64, diffMs);
        QFETCH(QString, expected);
        const qint64 now = 1'800'000'000'000;
        QCOMPARE(formatRelative(now - diffMs, now), expected);
    }
};

QTEST_APPLESS_MAIN(TstRelativeTime)
#include "tst_RelativeTime.moc"
