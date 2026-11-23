#include "ThemeManager.h"

#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QFontDatabase>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QHash>
#include <QPainter>
#include <QPalette>
#include <QPixmap>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QSettings>
#include <QStandardPaths>
#include <QStyleHints>
#include <QSvgRenderer>

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
    // Fusion draws disabled controls from the Disabled group, so dim their text explicitly.
    const QColor muted = tokenColor(mode, Token::TextMuted);
    for (QPalette::ColorRole role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText})
        palette.setColor(QPalette::Disabled, role, muted);
    qApp->setPalette(palette);
}

// Loads :/icons/<name>.svg, replaces currentColor with `color` and renders it into a 48 px pixmap
// (twice the nominal 24 px, so it stays sharp on high-density screens). Null when the icon is missing.
QPixmap renderIcon(const QString& name, const QColor& color)
{
    QFile file(":/icons/" + name + ".svg");
    if (!file.open(QIODevice::ReadOnly))
        return QPixmap();
    QString svg = QString::fromUtf8(file.readAll());
    // Opaque RGB is enough: SVG does not read the #AARRGGBB form the tokens may use.
    svg.replace("currentColor", color.name(QColor::HexRgb));

    constexpr int kPixels = 48;
    QPixmap pixmap(kPixels, kPixels);
    pixmap.fill(Qt::transparent);
    QSvgRenderer renderer(svg.toUtf8());
    QPainter painter(&pixmap);
    renderer.render(&painter);
    return pixmap;
}

// Index of the token with this name, or -1.
int tokenIndex(const QString& name)
{
    for (int i = 0; i < kTokenCount; ++i) {
        if (name == QLatin1String(kTokenNames[i]))
            return i;
    }
    return -1;
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
    // Icons first: the token pass would otherwise report "@icon" as an unknown token.
    qApp->setStyleSheet(resolveTokens(resolveIcons(css, mode), mode));

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

QIcon ThemeManager::icon(const QString& name, Token tint) const
{
    const QPixmap pixmap = renderIcon(name, color(tint));
    if (pixmap.isNull()) {
        qWarning().noquote() << "ThemeManager: icon not found" << name;
        return QIcon();
    }
    return QIcon(pixmap);
}

namespace {
// First folder that can hold the themed icon files: the cache folder, otherwise the temp folder. Empty when neither works.
QString usableIconFolder()
{
    QStringList candidates;
    const QString cache = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (!cache.isEmpty())   // an empty location would put the folder at the drive root
        candidates << cache + "/themed-icons";
    candidates << QDir::tempPath() + "/planova-themed-icons";
    for (const QString& folder : candidates) {
        if (QDir().mkpath(folder) && QFileInfo(folder).isDir() && QFileInfo(folder).isWritable())
            return folder;
    }
    return QString();
}

// Writes to a temporary file in the same folder and renames it over `path` when complete, so a second
// running copy of the app never reads a half-written PNG.
bool writePng(const QPixmap& pixmap, const QString& path)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    if (!pixmap.save(&file, "PNG")) {
        file.cancelWriting();
        return false;
    }
    return file.commit();
}
}

QString ThemeManager::resolveIcons(const QString& css, ThemeMode mode)
{
    // Spaces around the name and the token are allowed: @icon( check , AccentText ).
    static const QRegularExpression placeholder("@icon\\(\\s*([A-Za-z0-9_-]+)\\s*,\\s*([A-Za-z0-9]+)\\s*\\)");
    // Anything that still looks like @icon(...) after the pass below is malformed.
    static const QRegularExpression leftover("@icon\\([^)]*\\)");

    const QString folder = usableIconFolder();
    if (folder.isEmpty() && css.contains("@icon("))
        qWarning() << "ThemeManager: no writable cache or temp folder for themed icons";

    // Each icon file is rendered and written once per call, however often the stylesheet uses it.
    // The value is the text that replaces the placeholder: url("...") or, when the icon cannot be made, "none".
    // "none" is valid qss; the raw placeholder would make Qt drop the whole rule (a checked box would look unchecked).
    QHash<QString, QString> replacements;

    QString result;
    qsizetype last = 0;
    auto it = placeholder.globalMatch(css);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        const QString name = match.captured(1);
        const QString tokenName = match.captured(2);
        const QString key = name + "," + tokenName;
        if (!replacements.contains(key)) {
            QString replacement = "none";
            const int token = tokenIndex(tokenName);
            const QPixmap pixmap =
                token < 0 ? QPixmap() : renderIcon(name, tokenColor(mode, static_cast<Token>(token)));
            const QString path = folder + "/" + (mode == ThemeMode::Dark ? "dark" : "light") + "-" + name + "-"
                                 + tokenName + ".png";
            if (pixmap.isNull()) {
                qWarning().noquote() << "ThemeManager: cannot resolve" << match.captured(0);
            } else if (!folder.isEmpty()) {
                if (writePng(pixmap, path))
                    // Quoted, so a cache path with spaces is still one url(); qss wants forward slashes.
                    replacement = "url(\"" + QDir::fromNativeSeparators(path) + "\")";
                else
                    qWarning().noquote() << "ThemeManager: cannot write" << path;
            }
            replacements.insert(key, replacement);
        }
        result += css.mid(last, match.capturedStart() - last);
        result += replacements.value(key);
        last = match.capturedEnd();
    }
    result += css.mid(last);

    // "none" is valid qss; a raw "@icon(...)" would make Qt drop the whole rule.
    if (result.contains(leftover)) {
        qWarning().noquote() << "ThemeManager: malformed icon placeholder" << leftover.match(result).captured(0);
        result.replace(leftover, "none");
    }
    return result;
}

QString ThemeManager::resolveTokens(const QString& css, ThemeMode mode)
{
    // The pattern takes the whole name (letters and digits), so "@SurfaceAlt" is never
    // read as "@Surface" followed by "Alt". A url("...") is matched first and left alone: an icon cache path
    // may hold an "@" that is not a token.
    static const QRegularExpression placeholder("url\\(\"[^\"]*\"\\)|@([A-Za-z0-9]+)");

    QString result;
    qsizetype last = 0;
    QSet<QString> warned;
    auto it = placeholder.globalMatch(css);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        if (!match.hasCaptured(1))
            continue;   // a url("..."): it stays in the text
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

void ThemeManager::loadFonts()
{
    const QStringList files = QDir(":/fonts").entryList({"*.ttf", "*.otf"}, QDir::Files, QDir::Name);
    for (const QString& name : files) {
        if (QFontDatabase::addApplicationFont(":/fonts/" + name) < 0)
            qWarning().noquote() << "ThemeManager: could not load font" << name;
    }
}

QString ThemeManager::familyFor(FontRole role, const QStringList& available)
{
    // Candidates in order of preference. Windows 11 ships Segoe UI Variable; depending on the Qt version its
    // optical sizes are reported as separate families ("... Text", "... Display") or as one family.
    QStringList candidates;
    QString fallback = "Segoe UI";
    switch (role) {
    case FontRole::Body:
        candidates = {"Segoe UI Variable Text", "Segoe UI Variable", "Segoe UI"};
        break;
    case FontRole::Display:
        candidates = {"Segoe UI Variable Display", "Segoe UI Variable", "Segoe UI"};
        break;
    case FontRole::Mono:
        candidates = {"JetBrains Mono", "Consolas"};
        fallback = "Consolas";
        break;
    case FontRole::Script:
        candidates = {"Macintosh Skeletal Filled"};
        break;
    }
    for (const QString& family : candidates) {
        if (available.contains(family))
            return family;
    }
    return fallback;
}

QFont ThemeManager::font(FontRole role, int pixelSize) const
{
    QFont result(familyFor(role, QFontDatabase::families()));
    result.setPixelSize(pixelSize);
    result.setHintingPreference(QFont::PreferDefaultHinting);
    return result;
}

FontRole ThemeManager::roleFor(TextStyle style)
{
    switch (style) {
    case TextStyle::Heading:
    case TextStyle::PageTitle:
        return FontRole::Display;
    case TextStyle::Caption:
    case TextStyle::Body:
    case TextStyle::Label:
    case TextStyle::LabelActive:
        break;
    }
    return FontRole::Body;
}

QFont ThemeManager::textFont(TextStyle style) const
{
    int pixels = 14;
    QFont::Weight weight = QFont::Normal;
    switch (style) {
    case TextStyle::Caption:
        pixels = 12;
        weight = QFont::Medium;
        break;
    case TextStyle::Body:
        break;
    case TextStyle::Label:
        weight = QFont::Medium;
        break;
    case TextStyle::LabelActive:
        weight = QFont::DemiBold;
        break;
    case TextStyle::Heading:
        pixels = 16;
        weight = QFont::DemiBold;
        break;
    case TextStyle::PageTitle:
        pixels = 24;
        weight = QFont::DemiBold;
        break;
    }
    QFont result = font(roleFor(style), pixels);
    result.setWeight(weight);
    return result;
}
