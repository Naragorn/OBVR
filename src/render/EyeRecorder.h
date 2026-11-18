#pragma once

// The recording mode's Direct3D 9 side ([Debug] Record; the plan and its
// reasons in test/RecordingPlan.h): an eye shrunk on the GPU, copied into a
// small system-memory surface, read back frames later and written by a thread
// of its own, so the game keeps its pace while it is filmed.
//
// Everything here runs on the game's render thread except the writer. The
// writer's buffers are handed over by a state word each: free, filled (the
// render thread wrote the pixels and the name), being written.

#include "core/Types.h"
#include "test/RecordingPlan.h"

namespace obvr::render {

struct EyeRecorderStats {
	UInt32 filmed = 0;        // pictures copied on the GPU
	UInt32 written = 0;       // files the writer finished
	UInt32 skippedSlots = 0;  // frames dropped: every read-back slot busy (the writer behind)
	UInt32 failed = 0;        // a blit, copy, lock or file refused
};

// Once a frame, after the eye pictures are final for the submit: films the
// eye when the plan asks, reads back what the GPU finished, hands it to the
// writer. `eyeSurface` the eye's level-0 surface, `width`/`height`/`format`
// its size and D3DFORMAT (32-bit colour expected). True when this frame
// filmed a picture.
bool StepEyeRecorder(void* gameDevice, void* eyeSurface, UInt32 width, UInt32 height, UInt32 format,
                     const test::RecordSettings& settings, bool marked, const char* markName, float dt);

// What the recorder did so far, for the log.
EyeRecorderStats EyeRecorderTotals();

// The film's size as planned for the last frame filmed, 0 before any.
void EyeRecorderSize(UInt32& width, UInt32& height);

class HeadsetRenderer;

// The frame's step for the right eye as the headset gets it, with a line at
// every mark while recording: what was filmed, written and dropped so far.
void RecordRightEye(const HeadsetRenderer& renderer, void* gameDevice, const test::RecordSettings& settings,
                    bool marked, const char* markName, float dt);

}  // namespace obvr::render
