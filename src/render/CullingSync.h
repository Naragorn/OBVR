#pragma once

#include "core/Types.h"
#include "game/NiMath.h"

namespace obvr::render {

// The state machine behind sharing culling between the two eye renders.
// It contains no process memory or hook machinery, so every decision can be
// exercised by a native unit test.
enum class CullingSyncResult {
	Inactive,
	Captured,
	CaptureFull,
	Reused,
	CameraMismatch,
	NoCapturedCall,
};

struct CullingCameraSample {
	const void* camera = nullptr;
	NiPoint3 position{};
};

class CullingFrameSync {
public:
	static constexpr UInt32 kCapacity = 64;

	void BeginCapture() {
		m_mode = Mode::Capture;
		m_count = 0;
		m_index = 0;
	}

	void BeginReplay() {
		m_mode = Mode::Replay;
		m_index = 0;
	}

	void End() { m_mode = Mode::Off; }

	CullingSyncResult Visit(const void* camera, const NiPoint3& position,
	                        NiPoint3& replacement) {
		if (m_mode == Mode::Off) {
			return CullingSyncResult::Inactive;
		}
		if (m_mode == Mode::Capture) {
			if (m_count == kCapacity) {
				return CullingSyncResult::CaptureFull;
			}
			m_samples[m_count].camera = camera;
			m_samples[m_count].position = position;
			++m_count;
			return CullingSyncResult::Captured;
		}

		const UInt32 index = m_index++;
		if (index >= m_count) {
			return CullingSyncResult::NoCapturedCall;
		}
		if (m_samples[index].camera != camera) {
			return CullingSyncResult::CameraMismatch;
		}
		replacement = m_samples[index].position;
		return CullingSyncResult::Reused;
	}

	UInt32 CapturedCount() const { return m_count; }
	UInt32 ReplayCount() const { return m_index; }

private:
	enum class Mode { Off, Capture, Replay };

	Mode m_mode = Mode::Off;
	CullingCameraSample m_samples[kCapacity]{};
	UInt32 m_count = 0;
	UInt32 m_index = 0;
};

}  // namespace obvr::render
