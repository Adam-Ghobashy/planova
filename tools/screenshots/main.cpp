// Dev tool: saves one PNG per page and theme so the UI can be compared with the Figma prototype.
// Usage: planova_screenshots <outDir> [--theme light|dark|both] [--page N]
#include "app/CoreBridge.h"
#include "app/DemoData.h"
#include "app/MainWindow.h"
#include "planova/PlanovaCore.h"
#include "theme/ThemeManager.h"

#include <QApplication>
#include <QDir>
#include <QPixmap>
#include <QStackedWidget>
#include <QTemporaryDir>
#include <QTimer>

#include <cstdio>
#include <functional>
#include <filesystem>

namespace {

struct Options {
    QString outDir;
    QString theme = "light";
    int page = -1; // -1 means every page
};

bool parseArgs(const QStringList& args, Options& options)
{
    if (args.size() < 2)
        return false;
    options.outDir = args[1];
    for (int i = 2; i < args.size(); ++i) {
        if (args[i] == "--theme" && i + 1 < args.size())
            options.theme = args[++i];
        else if (args[i] == "--page" && i + 1 < args.size())
            options.page = args[++i].toInt();
        else
            return false;
    }
    return options.theme == "light" || options.theme == "dark" || options.theme == "both";
}

} // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    Q_INIT_RESOURCE(planova);

    Options options;
    if (!parseArgs(app.arguments(), options)) {
        std::fprintf(stderr, "usage: planova_screenshots <outDir> [--theme light|dark|both] [--page N]\n");
        return 2;
    }
    QDir().mkpath(options.outDir);

    // The first theme is applied before the window exists, so every widget is built under it.
    const QStringList themes = options.theme == "both" ? QStringList{"light", "dark"} : QStringList{options.theme};
    ThemeManager::instance().loadFonts();
    ThemeManager::instance().apply(themes.first() == "dark" ? ThemeMode::Dark : ThemeMode::Light);
    QApplication::setFont(ThemeManager::instance().font(FontRole::Body, 10));

    QTemporaryDir workspace;
    if (!workspace.isValid())
        return 1;
    PlanovaCore core(std::filesystem::path(workspace.path().toStdString()));
    core.openProject(core.createProject("Planova", "Team Six", "Agile", ""));
    demo::seedIfEmpty(core);

    CoreBridge bridge(core);
    MainWindow window(core, bridge);
    window.resize(1440, 900);
    window.show();

    auto* pages = window.findChild<QStackedWidget*>();
    if (!pages)
        return 1;

    // One entry per screenshot: (theme name, page index).
    QList<QPair<QString, int>> jobs;
    for (const QString& theme : themes) {
        for (int page = 0; page < pages->count(); ++page) {
            if (options.page < 0 || options.page == page)
                jobs.append({theme, page});
        }
    }

    int index = 0;
    // Show a page, wait for it to paint, grab it, then schedule the next one.
    std::function<void()> step = [&]() {
        if (index >= jobs.size()) {
            app.quit();
            return;
        }
        const auto job = jobs[index++];
        const ThemeMode mode = job.first == "dark" ? ThemeMode::Dark : ThemeMode::Light;
        if (ThemeManager::instance().mode() != mode)
            ThemeManager::instance().apply(mode);
        pages->setCurrentIndex(job.second);
        QTimer::singleShot(250, [&, job]() {
            const QString path = QString("%1/page%2-%3.png").arg(options.outDir).arg(job.second).arg(job.first);
            if (!window.grab().save(path))
                std::fprintf(stderr, "could not save %s\n", qPrintable(path));
            QTimer::singleShot(0, step);
        });
    };
    QTimer::singleShot(250, step);

    return app.exec();
}
