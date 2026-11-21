#include "app/SplashTimeline.h"
#include "app/SplashWindow.h"
#include "theme/ThemeManager.h"
#include <QImage>
#include <QKeyEvent>
#include <QSignalSpy>
#include <QTest>

namespace {
void compareFrames(const SplashFrame& a, const SplashFrame& b)
{
    QCOMPARE(a.windowOpacity, b.windowOpacity);
    QCOMPARE(a.wordReveal, b.wordReveal);
    QCOMPARE(a.markReveal, b.markReveal);
    QCOMPARE(a.subtitleOpacity, b.subtitleOpacity);
    QCOMPARE(a.subtitleRise, b.subtitleRise);
}
}

class tst_SplashTimeline : public QObject {
    Q_OBJECT
private slots:
    void initTestCase()
    {
        ThemeManager::instance().loadFonts();
        ThemeManager::instance().apply(ThemeMode::Light);
    }
    void cleanupTestCase() { ThemeManager::instance().apply(ThemeMode::Light); }

    void startsHidden()
    {
        const SplashFrame f = SplashTimeline::at(0);
        QCOMPARE(f.windowOpacity, 0.0);
        QCOMPARE(f.wordReveal, 0.0);
    }
    void wordWritesBetween150And1250()
    {
        QCOMPARE(SplashTimeline::at(150).wordReveal, 0.0);
        const double middle = SplashTimeline::at(700).wordReveal;
        QVERIFY(middle > 0.0 && middle < 1.0);
        QCOMPARE(SplashTimeline::at(1250).wordReveal, 1.0);
    }
    void markAfterWord()
    {
        QCOMPARE(SplashTimeline::at(1320).markReveal, 0.0);
        QCOMPARE(SplashTimeline::at(1540).markReveal, 1.0);
    }
    void subtitleFadesIn()
    {
        QCOMPARE(SplashTimeline::at(1450).subtitleOpacity, 0.0);
        QCOMPARE(SplashTimeline::at(1800).subtitleOpacity, 1.0);
        QCOMPARE(SplashTimeline::at(1800).subtitleRise, 0.0);
    }
    void fadesOutAtEnd()
    {
        QCOMPARE(SplashTimeline::at(1000).windowOpacity, 1.0);
        QCOMPARE(SplashTimeline::at(2400).windowOpacity, 0.0);
    }
    void clampsOutOfRange()
    {
        compareFrames(SplashTimeline::at(-5), SplashTimeline::at(0));
        compareFrames(SplashTimeline::at(9999), SplashTimeline::at(SplashTimeline::kTotalMs));
    }

    // No start(), so there is no screen grab: only the tint, edge and text are painted.
    void paintsWithoutBackdrop()
    {
        SplashWindow splash;
        const QImage image = splash.grab().toImage();
        QVERIFY(!image.isNull());
        bool anyVisible = false;
        for (int y = 0; y < image.height() && !anyVisible; ++y)
            for (int x = 0; x < image.width(); ++x)
                if (qAlpha(image.pixel(x, y)) > 0) { anyVisible = true; break; }
        QVERIFY(anyVisible);
    }
    void anyKeySkipsToTheEnd()
    {
        SplashWindow splash;
        QSignalSpy finished(&splash, &SplashWindow::finished);
        splash.start();
        QTest::keyClick(&splash, Qt::Key_Escape);
        // Skipping jumps to 2100 ms, so the 300 ms fade-out is all that is left.
        QVERIFY(finished.wait(1000));
        QCOMPARE(finished.count(), 1);
    }
};

QTEST_MAIN(tst_SplashTimeline)
#include "tst_SplashTimeline.moc"
