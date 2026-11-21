# Bundled fonts

| File | Family | Used for | Source | Licence |
|------|--------|----------|--------|---------|
| `Inter.ttf` | Inter (variable) | Body text | https://github.com/google/fonts/raw/main/ofl/inter/Inter%5Bopsz,wght%5D.ttf | SIL Open Font License 1.1, see `OFL-Inter.txt` |
| `SpaceGrotesk.ttf` | Space Grotesk (variable) | Page titles | https://github.com/google/fonts/raw/main/ofl/spacegrotesk/SpaceGrotesk%5Bwght%5D.ttf | SIL Open Font License 1.1, see `OFL-SpaceGrotesk.txt` |
| `JetBrainsMono.ttf` | JetBrains Mono (variable) | Numbers in stat cards, charts | https://github.com/google/fonts/raw/main/ofl/jetbrainsmono/JetBrainsMono%5Bwght%5D.ttf | SIL Open Font License 1.1, see `OFL-JetBrainsMono.txt` |
| `MacintoshSkeletalFilled.otf` | Macintosh Skeletal Filled | Decorative script (design reference only) | Copied from `GUI Reference (Figma)/assets/fonts/MacintoshSkeletalFilled.otf` | Not stated, see below |

Each `OFL-*.txt` was downloaded from the same folder of the Google Fonts repository as its font (`.../ofl/<family>/OFL.txt`).

## Macintosh Skeletal Filled: licence not stated

The `GUI Reference (Figma)/assets/fonts/` folder does not state a licence for this font. Its only terms file,
`ABSOLUTELY_VITAL_INFORMATION.txt`, belongs to a different font (Billion Dreams, `BillionDreams_PERSONAL.ttf`):

> This font is for PERSONAL USE ONLY and requires a license for commercial use.

That font is deliberately **not** bundled. The owner should confirm the licence of Macintosh Skeletal Filled before
Planova is distributed outside the team. If it cannot be redistributed, remove the file and its line in
`resources/planova.qrc`; the app then falls back to Segoe UI for the Script role. Also edit
`tst_ThemeManager::fontsRegistered`, which checks that this font is registered, or that test will fail.
