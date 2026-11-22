#include "app/DamagedFilesMessage.h"

#include <QTest>

#include <string>
#include <vector>

namespace {
std::vector<std::string> fileNames(int count)
{
    std::vector<std::string> names;
    for (int i = 1; i <= count; ++i)
        names.push_back("event-" + std::to_string(i) + ".json");
    return names;
}
} // namespace

class tst_DamagedFilesMessage : public QObject {
    Q_OBJECT
private slots:
    // One file: its name and the folder to look in.
    void oneFileNamesItAndTheFolder()
    {
        const QString text = damagedFilesMessage(fileNames(1), "C:/Docs/Planova");
        QVERIFY2(text.contains("1 file"), qPrintable(text));
        QVERIFY2(text.contains("event-1.json"), qPrintable(text));
        QVERIFY2(text.contains("C:/Docs/Planova") || text.contains("C:\\Docs\\Planova"), qPrintable(text));
    }

    // Up to five names are listed, the rest are counted.
    void longListShowsFiveAndCountsTheRest()
    {
        const QString text = damagedFilesMessage(fileNames(8), "C:/Docs/Planova");
        QVERIFY2(text.contains("8 files"), qPrintable(text));
        for (int i = 1; i <= 5; ++i)
            QVERIFY2(text.contains(QString("event-%1.json").arg(i)), qPrintable(text));
        QVERIFY2(!text.contains("event-6.json"), qPrintable(text));
        QVERIFY2(text.contains("and 3 more"), qPrintable(text));
    }

    void exactlyFiveHasNoMoreLine()
    {
        const QString text = damagedFilesMessage(fileNames(5), "C:/Docs/Planova");
        QVERIFY2(text.contains("event-5.json"), qPrintable(text));
        QVERIFY2(!text.contains("more"), qPrintable(text));
    }
};

QTEST_APPLESS_MAIN(tst_DamagedFilesMessage)
#include "tst_DamagedFilesMessage.moc"
