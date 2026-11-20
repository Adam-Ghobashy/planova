#include "ThemeManager.h"

#include <QApplication>
#include <QDir>
#include <QFile>

namespace {
QString readStyleSheet(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString();
    return QString::fromUtf8(file.readAll());
}
}

ThemeManager& ThemeManager::instance()
{
    static ThemeManager manager;
    return manager;
}

void ThemeManager::apply(ThemeMode)
{
    qApp->setStyle("Fusion");

    // base.qss first so page files can override it, then the rest by name.
    const QString dir = ":/themes/light";
    QString css = readStyleSheet(dir + "/base.qss");
    const QStringList files = QDir(dir).entryList({"*.qss"}, QDir::Files, QDir::Name);
    for (const QString& name : files) {
        if (name == "base.qss")
            continue;
        css += "\n" + readStyleSheet(dir + "/" + name);
    }
    qApp->setStyleSheet(css);
}

QColor ThemeManager::color(Token token) const
{
    return QColor(kLightHex[static_cast<int>(token)]);
}
