# Milestones

The plan comes from section 9 of `docs/planova/Planova_Team_Implementation_Plan.pdf` (relative timeline, no fixed
deadline). The last column says what was delivered.

## Three-week plan

| Period | Plan | Checkpoint in the plan | Delivered |
|--------|------|------------------------|-----------|
| Week 1, days 1-2 | Phase 0: S1 writes the shared contract (types, events, DTOs, query interfaces), everyone reviews, then it is frozen. | `common/` merged, empty Qt Test green | Done. `core/include/planova/common/` holds the contract. |
| Week 1, days 3-7 | Each slice: tests, events, projection, service. S1 also lands the theme and the main window. | `ctest` green per slice | Done for S1 to S6. Services validate and throw `std::invalid_argument`; projections implement the query interfaces. |
| Week 2 | Build each slice's Qt pages on real services. Draw the missing light dialogs first. | Every screen live, light mode | Done. All eight main screens are live on real data. New Task, Leave a Pin and Decline dialogs exist. |
| Week 3 | Integration tests, cross-slice flows, polish, docs, demo. | End-to-end test and demo script pass; grep check still clean | Done. `tests/integration/tst_EndToEnd.cpp` passes; docs written (this file, `SE_Process.md`, `README.md`). |
| Later | Dark mode: dark tokens, dark style sheets, enable the toggle. | n/a | Done in the full-completion stage. Both themes ship, with a toggle in the sidebar. |

## Cross-slice flows to demo

| Flow | Status |
|------|--------|
| Wizard to dashboard: create project, add members, create tasks, open Project Home | Done. The setup page creates the project, members and slicer weights; Home reads them. |
| Slice and apply: Slicer preview, Apply Assignments, cards with avatars on the Phase Board | Done. |
| Decline and recalculate: decline from a board card, blocker pin appears, Team shows Decline and Clash, Slicer reassigns, badge clears on resolve | Done. Members with an open decline conflict are skipped when recalculating. |
| Fair tracking: move tasks to Done, leaderboard and Home numbers update and agree | Done. Both derive from the same service outputs. |
| Git sync of data: pull a teammate's event files, restart, identical state | Done at file level: the end-to-end test reopens the same folder in a second `PlanovaCore` and gets identical state. A real `git pull` was not automated. |

## What was added after the original plan

- Dark theme and the theme toggle (spec 4.1 to 4.3).
- Bundled design fonts (Inter, Space Grotesk, JetBrains Mono) with fallbacks.
- Welcome splash and a Welcome page with two modes (intro and workspaces).
- Setup as a page (not a wizard window) with a slicer policy step.
- S5 backend and the live Pins page, real pin counts and sidebar badges.
- `ModalDialog` base class so every dialog and message uses the same style.
- Focus-ring audit (task 18b) and the keyboard-saved understanding slider with side-by-side GitHub lists (task 20b).
- Board column minimum of 260 px so four columns fit at 1440 px.

## Not done (out of scope)

Real GitHub provider, custom title bar, MSVC verification, sound effects. See "Known limitations" in `README.md`.
