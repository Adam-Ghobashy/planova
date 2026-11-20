#include <QApplication>
#include <QFile>
#include <QtTest>

#include "theme/ThemeManager.h"

class TstThemeManager : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() { Q_INIT_RESOURCE(planova); }

    void appliesEveryStylesheet() {
        ThemeManager::instance().apply(ThemeMode::Light);
        const QString css = qApp->styleSheet();
        for (const char* sel : {"QWidget#ChaosSlicerPage", "QWidget#homePage", "QWidget#PinsPage",
                                "#TrackingPage", "QLabel#PageTitle"})
            QVERIFY2(css.contains(sel), sel);
    }

    void comboPopupRulesAreScoped() {
        ThemeManager::instance().apply(ThemeMode::Light);
        // A bare rule would restyle every combo popup in the app.
        QVERIFY(!qApp->styleSheet().contains("\nQComboBox QAbstractItemView"));
    }

    void chevronIconIsBundled() { QVERIFY(QFile::exists(":/icons/chevron-updown.svg")); }
};

QTEST_MAIN(TstThemeManager)
#include "tst_ThemeManager.moc"
