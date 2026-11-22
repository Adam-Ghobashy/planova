#include "app/DamagedFilesMessage.h"

#include <QCoreApplication>
#include <QDir>
#include <QStringList>

namespace {
constexpr int kMaxListed = 5;   // a longer list would push the dialog off the screen
}

QString damagedFilesMessage(const std::vector<std::string>& fileNames, const QString& folder)
{
    const int count = static_cast<int>(fileNames.size());
    QString text = count == 1
        ? QCoreApplication::translate("DamagedFilesMessage",
                                      "1 file in the project folder is damaged and was skipped. "
                                      "Everything else loaded normally.")
        : QCoreApplication::translate("DamagedFilesMessage",
                                      "%1 files in the project folder are damaged and were skipped. "
                                      "Everything else loaded normally.")
              .arg(count);

    QStringList lines;
    for (int i = 0; i < count && i < kMaxListed; ++i)
        lines << QStringLiteral("• ") + QString::fromStdString(fileNames[static_cast<std::size_t>(i)]);
    if (count > kMaxListed)
        lines << QCoreApplication::translate("DamagedFilesMessage", "and %1 more").arg(count - kMaxListed);

    text += QStringLiteral("\n\n") + lines.join(QLatin1Char('\n'));
    text += QStringLiteral("\n\n")
            + QCoreApplication::translate("DamagedFilesMessage", "Folder: %1").arg(QDir::toNativeSeparators(folder));
    return text;
}
