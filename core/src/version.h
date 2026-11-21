// Port-only: the game's version and build time, shown on the title screen (MainMenu). The version
// is the repo's VERSION file (CMake passes it to version.cpp as DR_VERSION; Gradle reads it too).
#pragma once

// "0.1"
const char* GetVersion();

// "v0.1  Nov 12 2026 15:53:01": the version plus __DATE__ and __TIME__ of version.cpp's compilation.
const char* GetBuildInfo();
