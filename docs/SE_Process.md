# Software engineering process

This document describes how Planova was planned and built, and why the storage works the way it does. Course: Software
Engineering 2 + OOP, team of six.

## Process model: Agile / Scrum

We worked in short iterations with a working increment at the end of each one, using Scrum roles and events in a light
form:

| Scrum element | How we used it |
|---------------|----------------|
| Product backlog | The team implementation plan (`docs/planova/Planova_Team_Implementation_Plan.pdf`): six slices, each with screens, files and a definition of done. |
| Sprint | One week, three in total (see `docs/Milestones.md`). Each sprint ends with a checkpoint that must be green. |
| Sprint goal | Week 1: contract and backend. Week 2: every screen live. Week 3: integration, polish, docs, demo. |
| Increment | `ctest` passes and the app runs. A slice is not done until its tests pass. |
| Review | Every task is reviewed before the next one starts (code review plus a UI self-audit); a final review covers the whole branch. S1 owns the merge queue. |
| Definition of done | Per slice, listed in the plan. Examples: tests first for logic, no hex colours outside `Tokens.h`, `core/` links Qt6::Core only. |

Practices that support this:

- **Test first for logic.** Write the failing test, run it, implement, run again, commit.
- **One commit per task**, with conventional messages (`feat(pins): ...`, `fix(ui): ...`).
- **Shared contract first.** In Phase 0 the headers in `core/include/planova/common/` (types, events, DTOs, query
  interfaces) were written once and frozen. After that, slices build against the query interfaces and fakes, so they do
  not wait for each other.

## Why event sourcing

Planova has no database. Every change is written as one small JSON file (an event) and the current state is rebuilt by
replaying the files in order.

```
Qt page --calls--> Service --validates, then appends--> EventLog --> JSON files on disk
                                                           |
                                                           +--apply--> Projections (in-memory state)
Qt page <--reads-- Query interfaces <------------------------+
Qt page <-- CoreBridge::changed <-- ChangeBus <-- EventLog
```

Reasons for this design:

1. **Append-only files.** An event is never edited or deleted. History is complete, and replaying the same files always
   gives the same state (events are ordered by timestamp, then id).
2. **Git sync without merge conflicts.** Each event is its own file with a unique name
   (`<timestamp>-<hash8>.json`). Two teammates add different files, so git has nothing to merge. Committing, pushing and
   pulling the `events` folder is enough to share a project.
3. **No extra technology.** Qt Core can read and write JSON files, so the backend needs no database and no third-party
   library. This also fits the course rule of C++17 and Qt only.
4. **Easy to test.** A test creates a temporary folder, appends events through the services and checks the projections.
   `tests/integration/tst_EndToEnd.cpp` does this for a full project, then reopens the folder and checks the state is
   identical.

Costs we accepted: start-up replays every event (fine at this size), and a wrong event cannot be removed, so mistakes
are fixed by appending a new event.

Rules that keep it safe: services validate first and throw `std::invalid_argument` without appending anything, pages
never append events directly, and no exception may escape a Qt slot.

## The six slices

Each person owned one slice end to end: events, projection, service, tests, then the screens that use it. A slice talks
to another slice only through the query interfaces in `core/include/planova/common/Queries.h`.

| Slice | Owns (core) | Owns (UI) | Notes |
|-------|-------------|-----------|-------|
| S1 Foundation | `common/`, `store/` (event log, file store), `project/`, `PlanovaCore` | `ui/app` (main window, sidebar, splash, CoreBridge), `ui/theme`, `ui/welcome` (start popup, Projects Hub, setup page), shared `ui/widgets` (ModalDialog) | Wrote the Phase 0 contract and owns the merge queue. |
| S2 Team and GitHub | `team/` (members, skills, velocity), `git/` (simulated provider) | `ui/team`, `ui/git` | Velocity comes from `TaskMoved` events, with no dependency on S3 headers. |
| S3 Tasks and board | `task/` (status machine, dependency graph, critical path) | `ui/board` | Four-column Phase Board and the New Task dialog. A card menu can decline a task (calls S5). |
| S4 Chaos Slicer | `slicer/` (weighted scoring, pizza portions, saved weights) | `ui/slicer` | The scoring strategy is swappable through `IAssignmentStrategy`. |
| S5 Pins and conflicts | `pins/`, `conflict/` (decline, resolve) | `ui/pins` | Decline writes `TaskUnassigned` by event name, so S5 does not include S3 headers. |
| S6 Stats and docs | `stats/` (progress, leaderboard, home dashboard) | `ui/home`, `ui/tracking`, chart widgets (`StatCardWidget`, `DonutWidget`, `BarChartWidget`) | Writes no events. Home and Fair Tracking read the same service outputs, so they cannot disagree. |

Boundaries in short: `core/` never includes Qt GUI or Widgets headers; pages call services and read through query
interfaces; projections hold state; one slice never reaches into another slice's private headers.

## Theme system (light and dark)

- **Tokens.** Every colour literal lives in one file, `ui/theme/Tokens.h`, as two tables (`kLightHex`, `kDarkHex`). A
  token has a semantic name (`Background`, `Surface`, `TextPrimary`, `Accent`, `DangerSoft`, ...), not a colour name.
- **qss with placeholders.** The style sheets in `resources/themes/` write `@TokenName` instead of a hex value.
  `ThemeManager::resolveTokens` replaces the placeholders for the active mode, so one set of files serves both themes.
- **Painted widgets.** Custom paint code (pizza, donut, bar chart, splash) asks `ThemeManager::instance().color(Token::X)`
  and repaints on `ThemeManager::themeChanged`.
- **Toggle.** The sidebar toggle switches the mode and saves it in QSettings (`ui/theme`). On the first run the app
  follows the operating system colour scheme.
- **Guards.** `grep -rnE "#[0-9a-fA-F]{6}" core ui resources/themes | grep -v Tokens.h` must print nothing, and
  `tst_Contrast` checks text/background pairs for a WCAG contrast of at least 4.5:1 in both themes.
- **Keyboard focus.** Every control shows a visible focus ring in both themes. `tst_FocusRings` renders each control with
  and without focus and checks the ring contrast. Lessons learned: in dark mode `Accent` and `TextPrimary` are almost
  the same near-white, so a plain accent ring on an accent-filled button disappears (a focused accent button swaps its
  fill for `Surface` and gets a `TextPrimary` ring), and Qt's `3px double` border does not work in style sheets, so
  rings are plain 2 px or 3 px solid borders with the padding reduced to keep the size.

## How the work was executed

For this final stage (full completion) we followed a fixed loop:

1. **Spec**: the goal, what is in and out of scope, and the design of each part.
2. **Plan**: 22 tasks, each with files, steps (test
   first) and a check. Two tasks were added during the work: 18b (focus-ring audit) and 20b (keyboard slider and GitHub
   layout).
3. **Task by task**: one task, one commit. After each task: build, `ctest`, the hex grep, and for UI work a screenshot
   in both themes compared with the prototype in `GUI Reference (Figma)/`.
4. **Review** after every task, and a self-audit for UI tasks (component semantics, states and feedback, accessibility,
   token discipline).
5. **Final verification**: clean build, all tests, all grep checks, and a last screenshot pass over every page, dialog
   and the splash. Some visual notes are still open, see "Known limitations".

Prototype labels, first-person titles and database wording from the mock were removed from the UI. The mock data was
treated as a test fixture, not as truth.

## Known limitations

The final screenshot pass left these visual notes open. None of them blocks use of the app.

- Home, dark theme: the Done colour of the donut chart looks close to the To do colour.
- Home, dark theme: some tile icons are dim.
- Tracking: a member with 0 points still shows a filled stub bar.
- Dead space on Pins, Tracking, GitHub, Home and some Setup steps (the layout does not fill wide windows).
- The status bar uses the system font, not Inter.
