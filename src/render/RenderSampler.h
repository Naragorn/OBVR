#pragma once

#include "core/Types.h"

namespace obvr::render {

// A sampling profiler for the render thread, on while a performance capture
// runs and `[Performance] Sampler=1`: a thread wakes about every millisecond,
// suspends the render thread, reads its instruction pointer and the code
// addresses on its stack (the hang watchdog's way, core/StackScan.h), and
// resumes it. Each sample is one row of OBVR-samples.csv beside OBVR.log,
// written when the capture ends: the tick, whether a scene pass was running,
// the eip, and up to eight return addresses into Oblivion.exe's code from the
// stack upward (the innermost first). tools/perf-samples.py counts them per
// phase: where the eip was (exclusive) and which frames were on the stack
// (inclusive).
//
// Built 2026-10-19 for the question the call clocks could not answer: with
// five people in view a scene pass costs 8.3 ms, 3.3 of them inside the
// engine's per-shape render and only 1.9 inside the D3D9 calls - where the
// other 5 ms go, with no engine function known to hook, is what sampling
// says. About 20 to 50 us a sample at 1 kHz, a few percent of the frame,
// paid only while sampling.
void NoteRenderThread();   // on the render thread, once per process is enough
bool StartRenderSampler(); // the thread, when the setting asks for it

}  // namespace obvr::render
