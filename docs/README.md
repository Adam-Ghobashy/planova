# Planova

## Requirements

- Qt 6.8.3, MinGW 64-bit (`C:/Qt/6.8.3/mingw_64`)
- MinGW toolchain (`C:/Qt/Tools/mingw1310_64`), CMake 3.16 or newer, Ninja

## Build

Run from the repository root in Git Bash. The second command configures, the third builds and runs all tests.

```bash
export PATH="/c/Qt/6.8.3/mingw_64/bin:/c/Qt/Tools/mingw1310_64/bin:/c/Qt/Tools/CMake_64/bin:/c/Qt/Tools/Ninja:$PATH"
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH="C:/Qt/6.8.3/mingw_64"
cmake --build build && QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
```

`QT_QPA_PLATFORM=offscreen` lets the UI tests run without a screen.

## Run

Start `build/ui/planova.exe`.

Data is stored in `Documents/Planova`. The workspace root keeps the project list (`ProjectCreated` events). Each project writes its events to `Documents/Planova/<projectId>/events/*.json`. On start the app replays these events in (ts, id) order.

## Sync data with the team

Commit the `events/*.json` files of a project folder and push them. A teammate copies them into the same folder under their own `Documents/Planova`. When they restart the app, it replays the events and shows the same project.
