#include "perf/GpuQueries.h"

#include <new>

#include "render/D3D9Types.h"

namespace obvr::perf {

namespace d3d9 = obvr::render::d3d9;

bool GpuQueryMachine::Initialize(const QueryOps& ops, GpuSample* output,
                                 UInt32 outputCapacity, const GpuQueryConfig& config) {
	Shutdown();
	if (ops.context == nullptr || ops.create == nullptr || ops.issueEnd == nullptr ||
	    ops.getData == nullptr || ops.release == nullptr ||
	    output == nullptr || outputCapacity == 0 || config.slotCount == 0) {
		return false;
	}
	QueryOps resolved = ops;
	if (resolved.frequency == 0 && resolved.readFrequency != nullptr &&
	    !resolved.readFrequency(resolved.context, resolved.frequency)) return false;
	if (resolved.frequency == 0) return false;
	m_slots = new (std::nothrow) Slot[config.slotCount];
	if (m_slots == nullptr) return false;
	m_ops = resolved;
	m_output = output;
	m_outputCapacity = outputCapacity;
	m_slotCount = config.slotCount;
	m_timeoutPresents = config.timeoutPresents;
	for (UInt32 i = 0; i < m_slotCount; ++i) {
		if (!m_ops.create(m_ops.context, &m_slots[i].begin) ||
		    !m_ops.create(m_ops.context, &m_slots[i].end)) {
			Shutdown();
			return false;
		}
	}
	m_usable = true;
	return true;
}

bool GpuQueryMachine::Begin(const GpuSample& identity) {
	if (!m_usable || m_open) return false;
	UInt32 selected = 0xFFFFFFFFu;
	for (UInt32 i = 0; i < m_slotCount; ++i) {
		if (!m_slots[i].active) {
			selected = i;
			break;
		}
	}
	if (selected == 0xFFFFFFFFu) {
		++m_poolFull;
		return false;
	}
	Slot& slot = m_slots[selected];
	if (!m_ops.issueEnd(m_ops.context, slot.begin)) {
		++m_errors;
		return false;
	}
	slot.identity = identity;
	slot.identity.status = GpuStatus::Pending;
	slot.beganAtPresent = identity.presentId;
	slot.active = true;
	slot.ended = false;
	m_open = true;
	m_openSlot = selected;
	return true;
}

bool GpuQueryMachine::End() {
	if (!m_usable || !m_open || m_openSlot >= m_slotCount) return false;
	Slot& slot = m_slots[m_openSlot];
	const bool issued = m_ops.issueEnd(m_ops.context, slot.end);
	if (!issued) ++m_errors;
	slot.ended = issued;
	m_open = false;
	m_openSlot = 0xFFFFFFFFu;
	return issued;
}

UInt32 GpuQueryMachine::Poll(UInt64 presentId, UInt32 budget) {
	if (!m_usable || m_slotCount == 0 || budget == 0) return 0;
	UInt32 visited = 0;
	UInt32 completed = 0;
	while (visited < m_slotCount && completed < budget) {
		const UInt32 index = m_pollCursor++ % m_slotCount;
		++visited;
		Slot& slot = m_slots[index];
		if (!slot.active) continue;
		if (!slot.ended) {
			if (presentId - slot.beganAtPresent > m_timeoutPresents) {
				GpuSample sample = slot.identity;
				sample.status = GpuStatus::Timeout;
				sample.agePresents = static_cast<UInt32>(presentId - slot.beganAtPresent);
				if (m_outputCount < m_outputCapacity) m_output[m_outputCount++] = sample;
				slot.active = false;
				++m_errors; ++completed;
			}
			continue;
		}
		UInt64 begin = 0;
		UInt64 end = 0;
		const QueryPoll beginState = m_ops.getData(m_ops.context, slot.begin, begin);
		const QueryPoll endState = m_ops.getData(m_ops.context, slot.end, end);
		if (beginState == QueryPoll::Pending || endState == QueryPoll::Pending) {
			if (presentId - slot.beganAtPresent > m_timeoutPresents) {
				GpuSample sample = slot.identity;
				sample.status = GpuStatus::Timeout;
				sample.agePresents = static_cast<UInt32>(presentId - slot.beganAtPresent);
				if (m_outputCount < m_outputCapacity) m_output[m_outputCount++] = sample;
				slot.active = false;
				++m_errors; ++completed;
			}
			continue;
		}
		GpuSample sample = slot.identity;
		sample.startTick = begin;
		sample.endTick = end;
		sample.frequency = m_ops.frequency;
		sample.agePresents = static_cast<UInt32>(presentId - slot.beganAtPresent);
		if (beginState == QueryPoll::Error || endState == QueryPoll::Error) {
			sample.status = GpuStatus::QueryError;
			++m_errors;
		} else if (end < begin) {
			sample.status = GpuStatus::QueryError;
			++m_errors;
		} else {
			sample.status = GpuStatus::Ready;
		}
		if (m_outputCount < m_outputCapacity) m_output[m_outputCount++] = sample;
		slot.active = false;
		++completed;
	}
	m_pending = 0;
	for (UInt32 i = 0; i < m_slotCount; ++i) if (m_slots[i].active) ++m_pending;
	return completed;
}

void GpuQueryMachine::Stop() {
	if (!m_usable) return;
	for (UInt32 i = 0; i < m_slotCount; ++i) {
		Slot& slot = m_slots[i];
		if (!slot.active) continue;
		GpuSample sample = slot.identity;
		sample.status = GpuStatus::PendingAtStop;
		sample.agePresents = 0;
		if (m_outputCount < m_outputCapacity) m_output[m_outputCount++] = sample;
		slot.active = false;
	}
	m_pending = 0;
}

void GpuQueryMachine::Shutdown() {
	if (m_slots != nullptr && m_ops.release != nullptr) {
		for (UInt32 i = 0; i < m_slotCount; ++i) {
			if (m_slots[i].begin != nullptr) m_ops.release(m_ops.context, m_slots[i].begin);
			if (m_slots[i].end != nullptr) m_ops.release(m_ops.context, m_slots[i].end);
		}
	}
	delete[] m_slots;
	m_slots = nullptr;
	m_slotCount = 0;
	m_output = nullptr;
	m_outputCapacity = 0;
	m_outputCount = 0;
	m_usable = false;
	m_open = false;
	m_openSlot = 0xFFFFFFFFu;
	m_pending = 0;
}

namespace {

void D3D9Release(void*, void* query);

bool D3D9Create(void* context, void** query) {
	if (query == nullptr) return false;
	*query = nullptr;
	auto create = d3d9::Method<d3d9::CreateQueryFn>(context, d3d9::kDeviceCreateQuery);
	return create != nullptr && create(context, d3d9::kQueryTimestamp, query) >= 0 &&
	       *query != nullptr;
}

bool D3D9Issue(void*, void* query) {
	if (query == nullptr) return false;
	auto issue = d3d9::Method<d3d9::QueryIssueFn>(query, d3d9::kQueryIssue);
	return issue != nullptr && issue(query, d3d9::kIssueEnd) >= 0;
}

QueryPoll D3D9Get(void*, void* query, UInt64& value) {
	if (query == nullptr) return QueryPoll::Error;
	auto get = d3d9::Method<d3d9::QueryGetDataFn>(query, d3d9::kQueryGetData);
	if (get == nullptr) return QueryPoll::Error;
	const SInt32 result = get(query, &value, sizeof(value), 0);
	if (result == d3d9::kSOk) return QueryPoll::Ready;
	if (result == d3d9::kSFalse) return QueryPoll::Pending;
	return QueryPoll::Error;
}

bool D3D9ReadFrequency(void* context, UInt64& frequency) {
	frequency = 0;
	void* query = nullptr;
	auto create = d3d9::Method<d3d9::CreateQueryFn>(context, d3d9::kDeviceCreateQuery);
	if (create == nullptr || create(context, d3d9::kQueryTimestampFrequency, &query) < 0 ||
	    query == nullptr) return false;
	auto issue = d3d9::Method<d3d9::QueryIssueFn>(query, d3d9::kQueryIssue);
	UInt64 value = 0;
	const bool issued = issue != nullptr && issue(query, d3d9::kIssueEnd) >= 0;
	const QueryPoll state = issued ? D3D9Get(context, query, value) : QueryPoll::Error;
	D3D9Release(context, query);
	if (state != QueryPoll::Ready || value == 0) return false;
	frequency = value;
	return true;
}

void D3D9Release(void*, void* query) {
	if (query == nullptr) return;
	auto release = d3d9::Method<d3d9::ReleaseFn>(query, d3d9::kUnknownRelease);
	if (release != nullptr) release(query);
}

} // namespace

QueryOps MakeD3D9TimestampOps(void* device) {
	QueryOps ops{};
	ops.context = device;
	ops.create = &D3D9Create;
	ops.issueEnd = &D3D9Issue;
	ops.getData = &D3D9Get;
	ops.release = &D3D9Release;
	ops.readFrequency = &D3D9ReadFrequency;
	// D3D9 timestamp frequency is device-specific and is obtained once without
	// a blocking poll. A pending frequency query disables GPU timing for this
	// generation rather than inventing a conversion factor.
	ops.frequency = 0;
	return ops;
}

} // namespace obvr::perf
