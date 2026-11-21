#pragma once

// Pages of the main window. The stacked widget index is the enum value, so keep the order.
// Setup has no sidebar item. Hub is the Projects Hub (search, git banner, one card per project).
enum class Page { Hub, Setup, Slicer, Pins, Team, Board, Tracking, GitHub, Home };
