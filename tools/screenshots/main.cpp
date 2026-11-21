// Dev tool: saves one PNG per page and theme so the UI can be compared with the Figma prototype.
// Usage: planova_screenshots <outDir> [--theme light|dark|both] [--page N] [--dialogs] [--splash]
// --dialogs saves one PNG per dialog and theme instead (each dialog opened over the Board page).
// --splash saves the welcome splash, grabbed 1600 ms into its animation, once per theme.
#include "app/CoreBridge.h"
#include "app/DemoData.h"
#include "app/MainWindow.h"
#include "app/Page.h"
#include "app/Sidebar.h"
#include "app/SplashWindow.h"
#include "board/NewTaskDialog.h"
#include "pins/DeclineDialog.h"
#include "pins/LeavePinDialog.h"
#include "planova/PlanovaCore.h"
#include "theme/ThemeManager.h"
#include "widgets/ModalDialog.h"

#include <QApplication>
#include <QDir>
#include <QLabel>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QStackedWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QVBoxLayout>

#include <cstdio>
#include <functional>
#include <filesystem>

namespace {

struct Options {
    QString outDir;
    QString theme = "light";
    int page = -1; // -1 means every page
    bool dialogs = false;
    bool splash = false;
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
        else if (args[i] == "--splash")
            options.splash = true;
        else if (args[i] == "--dialogs")
            options.dialogs = true;
        else
            return false;
    }
    return options.theme == "light" || options.theme == "dark" || options.theme == "both";
}

// decline() is never reached: the screenshot only shows the dialog.
struct NoOpConflicts : planova::IConflictCommands {
    planova::Id decline(const planova::Id&, const planova::Id&, const std::string&) override { return {}; }
};

// A confirm or info box built by hand: ModalDialog::confirm() would block until it is answered.
ModalDialog* messageBox(QWidget* parent, const QString& title, const QString& text, const QString& okText,
                        ModalDialog::Role okRole, bool withCancel)
{
    auto* dlg = new ModalDialog(title, parent);
    auto* message = new QLabel(text);
    message->setObjectName("modalMessage");
    message->setWordWrap(true);
    dlg->contentLayout()->addWidget(message);
    if (withCancel)
        dlg->addButton("Cancel", ModalDialog::Role::Secondary);
    dlg->addButton(okText, okRole);
    return dlg;
}

} // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    Q_INIT_RESOURCE(planova);

    Options options;
    if (!parseArgs(app.arguments(), options)) {
        std::fprintf(stderr, "usage: planova_screenshots <outDir> [--theme light|dark|both] [--page N] [--dialogs] [--splash]\n");
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

    if (options.splash) {
        // The splash runs in real time; each theme gets a fresh one, grabbed mid-animation (subtitle fading in).
        constexpr int kGrabAtMs = 1600;
        int next = 0;
        std::function<void()> splashStep = [&]() {
            if (next >= themes.size()) {
                app.quit();
                return;
            }
            const QString theme = themes[next++];
            ThemeManager::instance().apply(theme == "dark" ? ThemeMode::Dark : ThemeMode::Light);
            auto* splash = new SplashWindow;
            splash->start();
            QTimer::singleShot(kGrabAtMs, [&, splash, theme]() {
                const QString path = QString("%1/splash-%2.png").arg(options.outDir, theme);
                if (!splash->grab().save(path))
                    std::fprintf(stderr, "could not save %s\n", qPrintable(path));
                splash->hide();
                splash->deleteLater();
                QTimer::singleShot(100, splashStep);
            });
        };
        QTimer::singleShot(250, splashStep);
        return app.exec();
    }

    if (options.dialogs) {
        // Dialogs sit on top of the Board page, as they do in the app.
        pages->setCurrentIndex(static_cast<int>(Page::Board));
        window.findChild<Sidebar*>()->setActivePage(Page::Board);
        NoOpConflicts conflicts;
        const auto tasks = core.taskQuery().all();
        const auto members = core.memberProjection().all();
        using Factory = std::function<ModalDialog*()>;
        const QList<QPair<QString, Factory>> dialogs = {
            {"new-task", [&] { return new NewTaskDialog(tasks, members, &window); }},
            {"leave-pin", [&] { return new planova::s5::LeavePinDialog(tasks, members, members.empty() ? planova::Id{} : members.front().id, &window); }},
            {"decline", [&] { return new planova::s5::DeclineDialog(tasks.empty() ? planova::Id{} : tasks.front().id, members.empty() ? planova::Id{} : members.front().id, "Implement Strategy Pattern", "Alice", conflicts, &window); }},
            {"confirm-danger", [&] { return messageBox(&window, "Delete pin", "Delete this pin? This cannot be undone.", "Delete", ModalDialog::Role::Danger, true); }},
            {"confirm-primary", [&] { return messageBox(&window, "Task declined", "Task declined. A blocker pin was added. To reassign, open the Slicer, press Recalculate, then Apply.", "Open Slicer", ModalDialog::Role::Primary, true); }},
            {"info", [&] { return messageBox(&window, "Cannot move task", "The task is blocked by an unfinished dependency.", "OK", ModalDialog::Role::Primary, false); }},
        };
        QList<QPair<QString, Factory>> shots;
        QStringList shotThemes;
        for (const QString& theme : themes) {
            for (const auto& d : dialogs) {
                shots.append(d);
                shotThemes.append(theme);
            }
        }
        int next = 0;
        std::function<void()> dialogStep = [&]() {
            if (next >= shots.size()) {
                app.quit();
                return;
            }
            const int current = next++;
            const ThemeMode mode = shotThemes[current] == "dark" ? ThemeMode::Dark : ThemeMode::Light;
            if (ThemeManager::instance().mode() != mode)
                ThemeManager::instance().apply(mode);
            ModalDialog* dlg = shots[current].second();
            dlg->open();   // shows it without blocking
            QTimer::singleShot(600, [&, dlg, current]() {
                // The dialog is a separate window, so the page is grabbed first and the dialog painted over it.
                QPixmap picture = window.grab();
                QPainter painter(&picture);
                painter.drawPixmap(0, 0, dlg->grab());
                painter.end();
                const QString path = QString("%1/dialog-%2-%3.png").arg(options.outDir, shots[current].first, shotThemes[current]);
                if (!picture.save(path))
                    std::fprintf(stderr, "could not save %s\n", qPrintable(path));
                dlg->close();
                dlg->deleteLater();
                QTimer::singleShot(100, dialogStep);
            });
        };
        QTimer::singleShot(250, dialogStep);
        return app.exec();
    }

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
        // The stack is switched directly, so tell the sidebar which item to highlight.
        window.findChild<Sidebar*>()->setActivePage(static_cast<Page>(job.second));
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
