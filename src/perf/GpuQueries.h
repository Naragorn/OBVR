#pragma once

#include "perf/ProfileLogic.h"

namespace obvr::perf {

enum class QueryPoll : UInt8 { Ready = 0, Pending, Error };

struct QueryOps {
	void* context = nullptr;
	bool (*create)(void* context, void** query) = nullptr;
	bool (*issueEnd)(void* context, void* query) = nullptr;
	QueryPoll (*getData)(void* context, void* query, UInt64& value) = nullptr;
	void (*release)(void* context, void* query) = nullptr;
	bool (*readFrequency)(void* context, UInt64& frequency) = nullptr;
	UInt64 frequency = 0;
};

struct GpuQueryConfig {
	UInt32 slotCount = 16;
	UInt32 timeoutPresents = 240;
};

// A fixed query pool. All polling is bounded and uses GetData flags=0, so a
// pending GPU result never flushes or blocks the game thread.
class GpuQueryMachine {
public:
	GpuQueryMachine() = default;
	~GpuQueryMachine() { Shutdown(); }
	GpuQueryMachine(const GpuQueryMachine&) = delete;
	GpuQueryMachine& operator=(const GpuQueryMachine&) = delete;

	bool Initialize(const QueryOps& ops, GpuSample* output, UInt32 outputCapacity,
	                const GpuQueryConfig& config = GpuQueryConfig{});
	bool IsUsable() const { return m_usable; }
	bool Begin(const GpuSample& identity);
	bool End();
	UInt32 Poll(UInt64 presentId, UInt32 budget);
	void Stop();
	void Shutdown();

	UInt32 PoolFullCount() const { return m_poolFull; }
	UInt32 PendingCount() const { return m_pending; }
	UInt32 ErrorCount() const { return m_errors; }
	UInt32 OutputCount() const { return m_outputCount; }

private:
	struct Slot {
		void* begin = nullptr;
		void* end = nullptr;
		GpuSample identity;
		UInt64 beganAtPresent = 0;
		bool active = false;
		bool ended = false;
	};

	QueryOps m_ops;
	Slot* m_slots = nullptr;
	GpuSample* m_output = nullptr;
	UInt32 m_slotCount = 0;
	UInt32 m_outputCapacity = 0;
	UInt32 m_outputCount = 0;
	UInt32 m_timeoutPresents = 240;
	UInt32 m_poolFull = 0;
	UInt32 m_pending = 0;
	UInt32 m_errors = 0;
	UInt32 m_pollCursor = 0;
	bool m_usable = false;
	bool m_open = false;
	UInt32 m_openSlot = 0xFFFFFFFFu;
};

// Adapter for an IDirect3DDevice9 reached through the existing raw vtable
// declarations. It only uses CreateQuery, Issue(END), GetData(flags=0), and
// Release; no new graphics work is issued by OBVR itself.
QueryOps MakeD3D9TimestampOps(void* device);

} // namespace obvr::perf
