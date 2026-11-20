#include "ThemeManager.h"

#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QPalette>
#include <QRegularExpression>
#include <QSet>
#include <QSettings>
#include <QStyleHints>

namespace {
QString readStyleSheet(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString();
    return QString::fromUtf8(file.readAll());
}

const char* const* hexTable(ThemeMode mode)
{
    return mode == ThemeMode::Dark ? kDarkHex : kLightHex;
}

QColor tokenColor(ThemeMode mode, Token token)
{
    return QColor(hexTable(mode)[static_cast<int>(token)]);
}

void applyPalette(ThemeMode mode)
{
    QPalette palette;
    const QColor text = tokenColor(mode, Token::TextPrimary);
    palette.setColor(QPalette::Window, tokenColor(mode, Token::Background));
    palette.setColor(QPalette::WindowText, text);
    palette.setColor(QPalette::Text, text);
    palette.setColor(QPalette::ButtonText, text);
    palette.setColor(QPalette::Base, tokenColor(mode, Token::Surface));
    palette.setColor(QPalette::AlternateBase, tokenColor(mode, Token::SurfaceAlt));
    palette.setColor(QPalette::Button, tokenColor(mode, Token::Surface));
    palette.setColor(QPalette::Highlight, tokenColor(mode, Token::Accent));
    palette.setColor(QPalette::HighlightedText, tokenColor(mode, Token::AccentText));
    palette.setColor(QPalette::ToolTipBase, tokenColor(mode, Token::Surface));
    palette.setColor(QPalette::ToolTipText, text);
    palette.setColor(QPalette::PlaceholderText, tokenColor(mode, Token::TextMuted));
    qApp->setPalette(palette);
}
}

ThemeManager& ThemeManager::instance()
{
    static ThemeManager manager;
    return manager;
}

void ThemeManager::apply(ThemeMode mode)
{
    m_mode = mode;
    qApp->setStyle("Fusion");
    applyPalette(mode);

    // base.qss first so page files can override it, then the rest by name.
    const QString dir = ":/themes";
    QString css = readStyleSheet(dir + "/base.qss");
    const QStringList files = QDir(dir).entryList({"*.qss"}, QDir::Files, QDir::Name);
    for (const QString& name : files) {
        if (name == "base.qss")
            continue;
        css += "\n" + readStyleSheet(dir + "/" + name);
    }
    qApp->setStyleSheet(resolveTokens(css, mode));

    emit themeChanged(mode);
}

ThemeMode ThemeManager::mode() const
{
    return m_mode;
}

void ThemeManager::toggle()
{
    apply(m_mode == ThemeMode::Light ? ThemeMode::Dark : ThemeMode::Light);
    saveMode();
}

QColor ThemeManager::color(Token token) const
{
    return tokenColor(m_mode, token);
}

QString ThemeManager::resolveTokens(const QString& css, ThemeMode mode)
{
    // The pattern takes the whole name (letters and digits), so "@SurfaceAlt" is never
    // read as "@Surface" followed by "Alt".
    static const QRegularExpression placeholder("@([A-Za-z0-9]+)");

    QString result;
    qsizetype last = 0;
    QSet<QString> warned;
    auto it = placeholder.globalMatch(css);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        const QString name = match.captured(1);
        bool known = false;
        for (int i = 0; i < kTokenCount; ++i) {
            if (name == QLatin1String(kTokenNames[i])) {
                result += css.mid(last, match.capturedStart() - last);
                result += QLatin1String(hexTable(mode)[i]);
                last = match.capturedEnd();
                known = true;
                break;
            }
        }
        // An unknown name stays in the text; warn once per name so a typo is easy to spot.
        if (!known && !warned.contains(name)) {
            warned.insert(name);
            qWarning().noquote() << "ThemeManager: unknown token @" + name;
        }
    }
    result += css.mid(last);
    return result;
}

ThemeMode ThemeManager::savedOrSystemMode()
{
    const QString saved = QSettings().value("ui/theme").toString();
    if (saved == "dark")
        return ThemeMode::Dark;
    if (saved == "light")
        return ThemeMode::Light;
    return QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark ? ThemeMode::Dark
                                                                                  : ThemeMode::Light;
}

void ThemeManager::saveMode() const
{
    QSettings().setValue("ui/theme", m_mode == ThemeMode::Dark ? "dark" : "light");
}
