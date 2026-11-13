#pragma once

// The crash report in OBVR.log (core/CrashReport.h says why and what).
//
// Installed on the first frame, not at the plugin's load: four other
// plugins on the tester's machine set their own unhandled exception filter
// (CobbCrashLogger, Modern Engine Fixes, GPEngineFixes, EngineBugFixes -
// their imports, 2026-11-01), and the last one set is the one Windows calls.
// Set after them, OBVR's runs first, writes its lines, and calls the filter
// it replaced - so a crash logger still writes its own report.

namespace obvr::crash {

// Sets the filter once; later calls do nothing. Safe to call every frame.
void InstallOnce();

// Called by the filter after OBVR's lines, before the filter it replaced:
// the performance ring's save (perf/Profiler.h SaveOnCrash). It must not use
// the heap. One; a later call replaces it, null removes it.
using CrashSaver = void (*)();
void SetCrashSaver(CrashSaver saver);

}  // namespace obvr::crash
