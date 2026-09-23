#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>

namespace obvr::game::vrmenu {

// The identity is the bridge's lifecycle identity, rather than a pointer. A
// replacement menu with the same type and root gets a new generation.
struct Identity {
	std::uint32_t type = 0;
	std::uint32_t root = 0;
	std::uint32_t generation = 0;
};

inline bool operator==(const Identity& a, const Identity& b) {
	return a.type == b.type && a.root == b.root && a.generation == b.generation;
}

inline bool operator!=(const Identity& a, const Identity& b) { return !(a == b); }

inline bool Valid(const Identity& identity) {
	return identity.type != 0 && identity.root != 0 && identity.generation != 0;
}

// This is the engine-facing snapshot supplied to the pure queue. Width and
// height are the pixel extent used by the native input adapter.
struct Snapshot {
	bool available = false;
	bool focused = false;
	bool open = false;
	bool interactive = false;
	Identity identity{};
	float width = 0;
	float height = 0;
};

inline bool Finite(float value) { return std::isfinite(value) != 0; }

// Availability and focus are the common lifecycle bridge checks. Identity is
// operation-specific: an opening world has no owned menu identity yet.
inline bool ValidLifecycle(const Snapshot& snapshot) {
	return snapshot.available && snapshot.focused;
}

inline bool ValidExtent(const Snapshot& snapshot) {
	// 16384 is a conservative extent ceiling. It keeps corrupted bridge data
	// from making an arbitrary coordinate look like a valid native pixel.
	return Finite(snapshot.width) && Finite(snapshot.height) &&
	       snapshot.width > 0 && snapshot.width <= 16384 &&
	       snapshot.height > 0 && snapshot.height <= 16384;
}

// Actual cursor/select input needs an open, interactive menu and a sane
// extent in addition to the lifecycle bridge.
inline bool Valid(const Snapshot& snapshot) {
	return ValidLifecycle(snapshot) && snapshot.open && snapshot.interactive &&
	       Valid(snapshot.identity) && ValidExtent(snapshot);
}

// A packet is deliberately plain data so a producer can build it without
// touching the game or an engine-owned object.
struct Input {
	Identity target{};
	bool cursor = false;
	float x = 0;
	float y = 0;
	bool held = false;
	int wheel = 0;
	bool cancel = false;
};

struct Output {
	bool accepted = false;
	bool cursor = false;
	float x = 0;
	float y = 0;
	int wheel = 0;
	bool selectDown = false;
	bool selectHeld = false;
	bool selectUp = false;
	bool cancel = false;
};

class InputQueue {
public:
	static constexpr std::size_t Capacity = 16;

	// Publish and Consume are intended to be serialized by the adapter. The
	// queue itself has no locks, allocation, or engine dependency.
	bool Publish(const Input& input) {
		if (m_releasePending) return false;
		if (m_count == Capacity) {
			// Never overwrite a packet while a click is held. The release edge is
			// kept in state outside the bounded FIFO and emitted before anything
			// else can be accepted.
			m_count = 0;
			m_read = 0;
			m_write = 0;
			m_armed = false;
			if (m_held) {
				m_held = false;
				m_releasePending = true;
			}
			return false;
		}
		m_packets[m_write] = input;
		m_write = (m_write + 1) % Capacity;
		++m_count;
		return true;
	}

	Output Consume(const Snapshot& snapshot) {
		if (m_releasePending) {
			m_releasePending = false;
			m_count = 0;
			m_read = 0;
			m_write = 0;
			m_armed = false;
			return ForcedRelease();
		}

		if (!Valid(snapshot) || !snapshot.open) {
			return FlushForLoss();
		}
		if (m_count == 0) return {};

		const Input input = m_packets[m_read];
		m_read = (m_read + 1) % Capacity;
		--m_count;

		if (!ValidInput(input, snapshot)) return FlushForLoss();

		if (input.cancel) {
			m_count = 0;
			m_read = 0;
			m_write = 0;
			m_armed = false;
			const bool wasHeld = m_held;
			m_held = false;
			Output output;
			output.cancel = true;
			output.selectUp = wasHeld;
			return output;
		}

		Output output;
		output.accepted = true;
		output.cursor = input.cursor;
		output.x = input.x;
		output.y = input.y;
		output.wheel = input.wheel;

		if (input.held) {
			// A packet that is already held when the bridge is first armed is
			// ignored as a click. A fresh neutral packet must precede a press.
			if (m_armed) {
				if (!m_held) output.selectDown = true;
				m_held = true;
			}
			output.selectHeld = m_held;
		} else {
			output.selectUp = m_held;
			m_held = false;
			m_armed = true;
		}
		return output;
	}

	// An explicit watchdog/native-dispatch release is intentionally a forced
	// edge. The adapter may call this again when the native release failed; a
	// stray release is safer than leaving the game with a stuck selection.
	Output Release() {
		m_count = 0;
		m_read = 0;
		m_write = 0;
		m_releasePending = false;
		m_armed = false;
		m_held = false;
		return ForcedRelease();
	}

	void Cancel() {
		m_count = 0;
		m_read = 0;
		m_write = 0;
		m_armed = false;
		if (m_held) {
			m_held = false;
			m_releasePending = true;
		}
	}

	std::size_t Pending() const { return m_count; }
	bool Held() const { return m_held; }

private:
	static bool ValidInput(const Input& input, const Snapshot& snapshot) {
		if (input.target != snapshot.identity || !Valid(input.target) ||
		    !Finite(input.x) || !Finite(input.y) || input.x < 0 ||
		    input.y < 0 || input.x >= snapshot.width || input.y >= snapshot.height ||
		    input.wheel < -4 || input.wheel > 4) return false;
		// A held click must have a surface hit. A neutral release is allowed to
		// pass through after the ray leaves the surface.
		if (input.held && !input.cursor) return false;
		if (input.wheel != 0 && !input.cursor) return false;
		return true;
	}

	Output FlushForLoss() {
		m_count = 0;
		m_read = 0;
		m_write = 0;
		m_armed = false;
		const bool wasHeld = m_held;
		m_held = false;
		Output output;
		output.selectUp = wasHeld;
		return output;
	}

	static Output ForcedRelease() {
		Output output;
		output.selectUp = true;
		return output;
	}

	Input m_packets[Capacity]{};
	std::size_t m_read = 0;
	std::size_t m_write = 0;
	std::size_t m_count = 0;
	bool m_armed = false;
	bool m_held = false;
	bool m_releasePending = false;
};

enum class Operation { None, OpenPersonalMenu, CloseOwnedMenu };

struct Request {
	std::uint64_t token = 0;
	Operation operation = Operation::None;
	Identity expected{};
};

struct Acknowledgement {
	std::uint64_t token = 0;
	Operation operation = Operation::None;
	bool observed = false;
	Identity identity{};
};

// Requests are commands sent to the adapter. They become state only when a
// later engine observation satisfies Acknowledged; dispatching a request is
// never treated as proof that the menu changed.
inline bool CanDispatch(const Request& request, const Snapshot& snapshot) {
	if (!ValidLifecycle(snapshot) || request.token == 0) return false;
	switch (request.operation) {
	case Operation::OpenPersonalMenu:
		return !snapshot.open && snapshot.identity == Identity{} &&
		       request.expected == Identity{};
	case Operation::CloseOwnedMenu:
		return snapshot.open && Valid(snapshot.identity) &&
		       request.expected == snapshot.identity;
	case Operation::None:
		return false;
	}
	return false;
}

inline bool Acknowledged(const Request& request, const Acknowledgement& acknowledgement) {
	if (request.token == 0 || request.operation == Operation::None ||
	    !acknowledgement.observed || request.token != acknowledgement.token ||
	    request.operation != acknowledgement.operation ||
	    !Valid(acknowledgement.identity)) return false;
	switch (request.operation) {
	case Operation::OpenPersonalMenu:
		return request.expected == Identity{};
	case Operation::CloseOwnedMenu:
		return Valid(request.expected) && acknowledgement.identity == request.expected;
	case Operation::None:
		return false;
	}
	return false;
}

// Integration obligations for the native adapter:
//  * serialize Publish/Consume/Cancel/Release on one thread;
//  * keep commands separate from observed acknowledgements and verify the
//    identity generation on every observation;
//  * if native injection fails, retry Release until neutral input is known to
//    have reached the game. No engine call is hidden in this pure bridge.

} // namespace obvr::game::vrmenu
