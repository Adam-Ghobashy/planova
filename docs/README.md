# Planova

Planova is a desktop app that helps a student team plan a project: members and skills, a phase board, a "Chaos Slicer"
that splits tasks fairly, pins for handoffs, fair tracking and a simulated GitHub page. It is written in C++17 with
Qt 6 Widgets. There is no database: every change is stored as an append-only event file.

More documents: [SE process](SE_Process.md), [milestones](Milestones.md),
[architecture](architecture/Planova_Full_Architecture_Audit.md).

## Requirements

- Windows with Qt 6.8.3, MinGW 64-bit (`C:/Qt/6.8.3/mingw_64`)
- MinGW toolchain (`C:/Qt/Tools/mingw1310_64`), CMake 3.16 or newer, Ninja

## Build and test

Run from the repository root in Git Bash. The first command sets the path, the second configures (first time only), the
third builds and runs all tests.

```bash
export PATH="/c/Qt/Tools/CMake_64/bin:/c/Qt/Tools/Ninja:/c/Qt/Tools/mingw1310_64/bin:/c/Qt/6.8.3/mingw_64/bin:$PATH"
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=C:/Qt/6.8.3/mingw_64
cmake --build build && (cd build && ctest --output-on-failure)
```

Tests use Qt Test and run through `ctest` (48 tests). The UI tests open real windows, so run them on a desktop session.
They load the bundled fonts and set Inter 10 pt as the app font (`tests/TestAppSetup.h`), because layout checks such as
card widths and click targets depend on font metrics. With that in place the whole suite also passes with
`QT_QPA_PLATFORM=offscreen`; a few hover and focus checks skip themselves there when the platform cannot simulate them.

## Run

Start `build/ui/planova.exe` (the Qt DLLs are copied next to it, so a double-click works).

| Flag | Effect |
|------|--------|
| (none) | Opens the most recent project. A new workspace starts on the Welcome page. |
| `--demo` | Fills an empty project with a sample team, tasks and pins. Use it to look around. |
| `--help` | Lists the flags. |

On start a short "Welcome" splash plays, then the main window fades in.

### Theme

The "Dark mode" / "Light mode" toggle at the bottom of the sidebar (moon and sun icon) switches between light and dark. The choice is saved with QSettings
(`ui/theme`). On the first run the app follows the operating system colour scheme.

### Data location

Data is stored in `Documents/Planova`. The workspace root keeps the project list (`ProjectCreated` events). Each project
writes its events to `Documents/Planova/<projectId>/events/*.json`. On start the app replays these events in (ts, id)
order.

## Fonts and licences

The fonts are bundled in `resources/fonts/` (see the README there for sources).

| Font | Used for | Licence |
|------|----------|---------|
| Inter | Body text | SIL Open Font License 1.1 (`OFL-Inter.txt`) |
| Space Grotesk | Page titles | SIL Open Font License 1.1 (`OFL-SpaceGrotesk.txt`) |
| JetBrains Mono | Numbers in stat cards and charts | SIL Open Font License 1.1 (`OFL-JetBrainsMono.txt`) |
| Macintosh Skeletal Filled | Decorative script (splash and welcome handwriting) | **Not stated** in the source folder |

The licence of Macintosh Skeletal Filled is not stated anywhere in the design reference. The owner must confirm it before
Planova is distributed outside the team. If it cannot be redistributed, remove the file and its line in
`resources/planova.qrc`; the app then falls back to Segoe UI for that role. If a font file is missing, the app falls back
to system fonts and logs a warning.

## Sync data with the team

Event files are append-only and each has a unique name, so git never has a merge conflict on them.

1. Commit the `events/*.json` files of the project folder and push them.
2. A teammate copies (or pulls) them into the same folder under their own `Documents/Planova`.
3. The teammate also needs the project's `ProjectCreated` event file from the workspace root (its payload `projectId`
   equals the folder name). Without it the project does not appear in the Projects Hub.
4. Restart the app. It replays the events and shows the same project.

Data from an older version of the app (members, git and task events in the workspace root) is copied into a project
folder once, the first time that project is opened and has no `events` folder yet.

## Screenshot tool

`planova_screenshots` saves PNGs of the UI so it can be compared with the prototype in `GUI Reference (Figma)/`.

```bash
build/tools/screenshots/planova_screenshots.exe <outDir> [options]
```

| Option | Effect |
|--------|--------|
| `--theme light\|dark\|both` | Theme to shoot (default `light`). |
| `--page N` | Only one page (default: every page). |
| `--width N`, `--height N` | Window size (default 1440x900). |
| `--dialogs` | One PNG per dialog and theme, opened over the Board. |
| `--card-menu` | Board with a focused task card and its menu open. |
| `--clash` | Make the first member the lead and add a decline clash, so the Lead chip and clash badge show. |
| `--setup-steps` | Every step of the Setup page, plus its error and blocked states. |
| `--welcome-states` | Welcome page with no projects, with no search match, and with focus on a card. |
| `--splash` | The splash, grabbed 1600 ms into its animation. |
| `--connect` | Connect a sample repository and run one sync, so the GitHub page has data. |
| `--empty` | Skip the sample data, to check empty states. |

## Known limitations

- **GitHub is simulated.** The commits, issues and notifications come from a built-in provider that uses your team's
  member names. There is no real GitHub provider. The page keeps its notice until one exists.
- **Reduced motion is not detected.** Qt has no portable way to read the system setting, so the splash and chart
  animations always play.
- **Home and Team layout limits.** These pages are designed for a window of about 1440 px wide. Narrower windows can
  clip or compress some cards, and a very large team makes the Team page long.
- **Slicer legend pills are mouse-only.** Clicking a legend pill or a slice highlights that member in the table;
  there is no keyboard equivalent (the table itself and every button are reachable with Tab).
- **Macintosh Skeletal Filled licence is unconfirmed** (see Fonts above).
- **Windows and MinGW only.** MSVC builds were not verified. The app uses the native window frame.
