// Dev tool: saves one PNG per page and theme so the UI can be compared with the Figma prototype.
// Usage: planova_screenshots <outDir> [--theme light|dark|both] [--page N]  [--clash] [--dialogs] [--splash] 
// --height N / --width N set the window size (default 1440x900), to see the bottom or the right side of a page.
// --empty skips the demo data, to check the empty-project look.
// --dialogs saves one PNG per dialog and theme instead (each dialog opened over the Board page).
// --setup-steps saves every step of the Setup page (filled with sample data) and its error and blocked states.
// --welcome-states saves the Welcome page with no projects, with no search match and with focus on a card.
// --splash saves the welcome splash, grabbed 1600 ms into its animation, once per theme.
// --card-menu saves the Board with one task card focused and its context menu open, once per theme.
#include "app/CoreBridge.h"
#include "app/DemoData.h"
#include "app/MainWindow.h"
#include "app/Page.h"
#include "app/Sidebar.h"
#include "app/SplashWindow.h"
#include "board/NewTaskDialog.h"
#include "board/TaskCardWidget.h"
#include "pins/DeclineDialog.h"
#include "pins/LeavePinDialog.h"
#include "planova/PlanovaCore.h"
#include "theme/ThemeManager.h"
#include "welcome/SetupPage.h"
#include "welcome/WelcomePage.h"
#include "widgets/ModalDialog.h"

#include <QApplication>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QRegularExpression>
#include <QSlider>
#include <QDir>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
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
    bool cardMenu = false;
    bool empty = false;
    bool clash = false;
    bool welcomeStates = false;
    bool setupSteps = false;
    int height = 0; // 0 keeps the default window height
    int width = 0;  // 0 keeps the default window width
};

bool parseArgs(const QStringList& args, Options& options)
{
    if (args.size() < 2)
        return false;
    options.outDir = args[1];
    for (int i = 2; i < args.size(); ++i) {
        if (args[i] == "--theme" && i + 1 < args.size())
            options.theme = args[++i];
        else if (args[i] == "--width" && i + 1 < args.size())
            options.width = args[++i].toInt();
        else if (args[i] == "--height" && i + 1 < args.size())
            options.height = args[++i].toInt();
        else if (args[i] == "--page" && i + 1 < args.size())
            options.page = args[++i].toInt();
        else if (args[i] == "--clash")
            options.clash = true;
        else if (args[i] == "--empty")
            options.empty = true;
        else if (args[i] == "--setup-steps")
            options.setupSteps = true;
        else if (args[i] == "--welcome-states")
            options.welcomeStates = true;
        else if (args[i] == "--splash")
            options.splash = true;
        else if (args[i] == "--card-menu")
            options.cardMenu = true;
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
    // Two more workspaces, so the Welcome page shows a row of cards. They are never opened.
    core.createProject("Library Portal", "ECU Team", "Scrum", "");
    core.createProject("Campus Navigator", "Wayfinders", "Waterfall", "");
    if (options.clash) // the first demo member becomes the lead, so the Lead chip shows
        core.members().add("Zeyad Waled", "Team & Systems Lead", true);
    if (!options.empty)
        demo::seedIfEmpty(core);
    if (options.clash) {
        // The first member declines a task, so the Team page shows a clash.
        core.members().setSetting("autoMatch", true);
        const auto firstMember = core.memberProjection().all().front().id;
        for (const TaskDto& task : core.taskQuery().all()) {
            if (task.assignee && *task.assignee == firstMember) {
                core.conflicts().decline(task.id, *task.assignee, "Overloaded this week");
                break;
            }
        }
    }

    CoreBridge bridge(core);
    MainWindow window(core, bridge);
    window.resize(options.width > 0 ? options.width : 1440, options.height > 0 ? options.height : (options.clash ? 1800 : 900)); // --clash is tall so every Team card is visible
    window.show();

    auto* pages = window.findChild<QStackedWidget*>();
    if (!pages)
        return 1;

    if (options.welcomeStates) {
        // The Welcome states the normal run cannot reach: no projects, no search match, keyboard focus on a card.
        QTemporaryDir emptyWorkspace;
        PlanovaCore emptyCore(std::filesystem::path(emptyWorkspace.path().toStdString()));
        CoreBridge emptyBridge(emptyCore);
        WelcomePage emptyPage(emptyCore, emptyBridge);
        WelcomePage fullPage(core, bridge);
        for (WelcomePage* page : {&emptyPage, &fullPage}) {
            page->resize(1100, 700);
            page->setMode(WelcomePage::Mode::Projects);
            page->show();
        }
        for (const QString& theme : themes) {
            ThemeManager::instance().apply(theme == "dark" ? ThemeMode::Dark : ThemeMode::Light);
            QApplication::processEvents();
            emptyPage.grab().save(QString("%1/welcome-empty-%2.png").arg(options.outDir, theme));
            fullPage.findChild<QLineEdit*>("projectSearch")->setText("zzz");
            QApplication::processEvents();
            fullPage.grab().save(QString("%1/welcome-nomatch-%2.png").arg(options.outDir, theme));
            fullPage.findChild<QLineEdit*>("projectSearch")->clear();
            // Tab from the search box: Start New Project, Manage Git, then the first card's Open button.
            auto* open = fullPage.findChildren<QPushButton*>("openProject").first();
            open->setFocus(Qt::TabFocusReason);
            QApplication::processEvents();
            fullPage.grab().save(QString("%1/welcome-focus-%2.png").arg(options.outDir, theme));
        }
        return 0;
    }

    if (options.setupSteps) {
        // Every step of the Setup page, filled with sample data, plus its empty, error and blocked states.
        QTemporaryDir setupWorkspace;
        PlanovaCore setupCore(std::filesystem::path(setupWorkspace.path().toStdString()));
        SetupPage page(setupCore);
        page.resize(1100, 1000);
        page.show();
        auto click = [&](const char* name) { page.findChild<QPushButton*>(name)->click(); QApplication::processEvents(); };
        auto grabTo = [&](const QString& name, const QString& theme) {
            QApplication::processEvents();
            const QString path = QString("%1/setup-%2-%3.png").arg(options.outDir, name, theme);
            if (!page.grab().save(path))
                std::fprintf(stderr, "could not save %s\n", qPrintable(path));
        };
        auto addMember = [&](const QString& name, bool lead, int backendRating) {
            click("addMemberButton");
            const auto edits = page.findChildren<QLineEdit*>(QRegularExpression("^memberNameEdit_"));
            QLineEdit* edit = edits.last();
            edit->setText(name);
            const QString id = edit->objectName().section('_', 1);
            page.findChild<QPushButton*>("memberLead_" + id)->setChecked(lead);
            page.findChild<QSlider*>(QString("skillSlider_%1_0").arg(id))->setValue(backendRating);
            return edit;
        };
        for (const QString& theme : themes) {
            ThemeManager::instance().apply(theme == "dark" ? ThemeMode::Dark : ThemeMode::Light);
            page.reset();
            // 1: empty (hint, Next disabled), then the name box left empty (error), then filled in
            grabTo("1-foundations-empty", theme);
            auto* nameEdit = page.findChild<QLineEdit*>("projectNameEdit");
            QMetaObject::invokeMethod(nameEdit, "editingFinished");
            grabTo("1-foundations-error", theme);
            nameEdit->setText("Library Portal");
            page.findChild<QLineEdit*>("teamNameEdit")->setText("ECU Team");
            page.findChild<QPlainTextEdit*>("ideaEdit")->setPlainText("A portal where students can search and loan books.");
            grabTo("1-foundations", theme);
            click("nextButton");
            // 2: Scrum chosen, keyboard focus on the chosen card
            page.findChild<QPushButton*>("modelCard_Scrum")->click();
            page.findChild<QPushButton*>("modelCard_Scrum")->setFocus(Qt::TabFocusReason);
            grabTo("2-model", theme);
            click("nextButton");
            // 3: no members yet, then two members and a third without a name (error)
            grabTo("3-team-empty", theme);
            addMember("Ana Hassan", true, 5);
            addMember("Ben Carter", false, 2);
            auto* unnamed = addMember("", false, 3);
            QMetaObject::invokeMethod(unnamed, "editingFinished");
            grabTo("3-team", theme);
            page.findChild<QPushButton*>(QString("memberRemove_%1").arg(unnamed->objectName().section('_', 1)))->click();
            click("nextButton");
            grabTo("4-slicer", theme);
            click("nextButton");
            page.findChild<QLineEdit*>("repoUrlEdit")->setText("https://github.com/ecu-team/library-portal.git");
            grabTo("5-git", theme);
            click("nextButton");
            grabTo("6-review", theme);
            // 6 again with no members: Create is disabled and the page says why
            for (int i = 0; i < 3; ++i)
                click("backButton");
            for (QPushButton* remove : page.findChildren<QPushButton*>(QRegularExpression("^memberRemove_")))
                remove->click();
            for (int i = 0; i < 3; ++i)
                click("nextButton");
            grabTo("6-review-blocked", theme);
        }
        return 0;
    }

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

    if (options.cardMenu) {
        pages->setCurrentIndex(static_cast<int>(Page::Board));
        window.findChild<Sidebar*>()->setActivePage(Page::Board);
        int next = 0;
        std::function<void()> menuStep = [&]() {
            if (next >= themes.size()) {
                app.quit();
                return;
            }
            const QString theme = themes[next++];
            ThemeManager::instance().apply(theme == "dark" ? ThemeMode::Dark : ThemeMode::Light);
            TaskCardWidget* card = nullptr;
            for (TaskCardWidget* c : window.findChildren<TaskCardWidget*>()) {
                if (!c->actions().isEmpty()) {
                    card = c;
                    break;
                }
            }
            if (!card) {
                std::fprintf(stderr, "no assigned task card on the board\n");
                app.quit();
                return;
            }
            card->setFocus(Qt::TabFocusReason);
            auto* menu = new QMenu(&window);
            menu->addActions(card->actions());
            menu->popup(card->mapToGlobal(QPoint(card->width() / 2, card->height() / 2)));
            QTimer::singleShot(600, [&, menu, theme]() {
                // The menu is its own window: grab the board, then paint the menu over it.
                QPixmap picture = window.grab();
                QPainter painter(&picture);
                painter.drawPixmap(window.mapFromGlobal(menu->pos()), menu->grab());
                painter.end();
                const QString path = QString("%1/board-card-menu-%2.png").arg(options.outDir, theme);
                if (!picture.save(path))
                    std::fprintf(stderr, "could not save %s\n", qPrintable(path));
                menu->close();
                menu->deleteLater();
                QTimer::singleShot(100, menuStep);
            });
        };
        QTimer::singleShot(250, menuStep);
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
        // Welcome and Hub are one widget (see MainWindow::showPage): index 1 shows it in projects mode.
        auto* welcomePage = window.findChild<WelcomePage*>();
        const bool welcomeShot = job.second <= static_cast<int>(Page::Hub);
        if (welcomeShot) {
            welcomePage->setMode(job.second == static_cast<int>(Page::Hub) ? WelcomePage::Mode::Projects
                                                                              : WelcomePage::Mode::Intro);
            pages->setCurrentIndex(static_cast<int>(Page::Welcome));
        } else {
            pages->setCurrentIndex(job.second);
        }
        // The stack is switched directly, so tell the sidebar which item to highlight.
        window.findChild<Sidebar*>()->setActivePage(static_cast<Page>(job.second));
        // The intro writes "Welcome!" on for about 1.9 s; wait for it to finish before grabbing.
        const int waitMs = job.second == static_cast<int>(Page::Welcome) ? 2300 : 250;
        QTimer::singleShot(waitMs, [&, job]() {
            const QString path = QString("%1/page%2-%3.png").arg(options.outDir).arg(job.second).arg(job.first);
            if (!window.grab().save(path))
                std::fprintf(stderr, "could not save %s\n", qPrintable(path));
            QTimer::singleShot(0, step);
        });
    };
    QTimer::singleShot(250, step);

    return app.exec();
}
