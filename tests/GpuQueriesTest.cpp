#include <cstdio>

#include "perf/GpuQueries.h"

namespace {
struct FakeQuery {
	bool used = false;
	bool ended = false;
	UInt64 value = 0;
	UInt32 polls = 0;
};
struct FakeDevice {
	FakeQuery queries[32];
	UInt32 created = 0;
	UInt32 released = 0;
	UInt32 issueCount = 0;
	UInt32 pendingPolls = 0;
	UInt32 failCreateAfter = 0xFFFFFFFFu;
	bool failIssue = false;
	bool failGet = false;
};

bool Create(void* context, void** query) {
	auto& d = *static_cast<FakeDevice*>(context);
	if (d.created >= d.failCreateAfter || d.created >= 32) return false;
	auto& q = d.queries[d.created++];
	q = FakeQuery{};
	q.used = true;
	q.value = static_cast<UInt64>(d.issueCount + 1) * 100;
	*query = &q;
	return true;
}
bool Issue(void* context, void* query) {
	auto& d = *static_cast<FakeDevice*>(context);
	if (d.failIssue || query == nullptr) return false;
	++d.issueCount;
	auto& q = *static_cast<FakeQuery*>(query);
	q.ended = true;
	q.value = static_cast<UInt64>(d.issueCount) * 100;
	return true;
}
obvr::perf::QueryPoll Get(void* context, void* query, UInt64& value) {
	auto& d = *static_cast<FakeDevice*>(context);
	if (d.failGet || query == nullptr) return obvr::perf::QueryPoll::Error;
	auto& q = *static_cast<FakeQuery*>(query);
	if (q.polls++ < d.pendingPolls) return obvr::perf::QueryPoll::Pending;
	value = q.value;
	return obvr::perf::QueryPoll::Ready;
}
void Release(void* context, void* query) {
	auto& d = *static_cast<FakeDevice*>(context);
	if (query != nullptr) ++d.released;
}

obvr::perf::QueryOps Ops(FakeDevice& d) {
	obvr::perf::QueryOps ops{};
	ops.context = &d;
	ops.create = &Create;
	ops.issueEnd = &Issue;
	ops.getData = &Get;
	ops.release = &Release;
	ops.frequency = 1000000;
	return ops;
}

int failures = 0;
void Check(bool condition, const char* text) {
	std::printf(condition ? "  ok    %s\n" : "  FAIL  %s\n", text);
	if (!condition) ++failures;
}

obvr::perf::GpuSample Identity(UInt64 id, UInt64 present) {
	obvr::perf::GpuSample sample{};
	sample.sampleId = id;
	sample.presentId = present;
	sample.type = obvr::perf::EventType::ScenePass;
	return sample;
}

void TestReadyAndPending() {
	FakeDevice d{};
	obvr::perf::GpuSample output[4]{};
	obvr::perf::GpuQueryMachine machine;
	Check(machine.Initialize(Ops(d), output, 4), "query pool initializes");
	Check(machine.Begin(Identity(7, 1)), "a slot accepts a marker pair");
	Check(machine.End(), "the end marker is issued");
	d.pendingPolls = 1;
	Check(machine.Poll(2, 4) == 0, "pending data is never forced ready");
	Check(machine.Poll(3, 4) == 1, "a later poll consumes the ready pair");
	Check(output[0].sampleId == 7 && output[0].status == obvr::perf::GpuStatus::Ready,
	      "ready output keeps its original sample id");
	machine.Shutdown();
	Check(d.released == 32, "the fixed query pool releases every handle once");
}

void TestPoolAndTimeout() {
	FakeDevice d{};
	obvr::perf::GpuSample output[4]{};
	obvr::perf::GpuQueryMachine machine;
	obvr::perf::GpuQueryConfig config{};
	config.slotCount = 1;
	config.timeoutPresents = 2;
	Check(machine.Initialize(Ops(d), output, 4, config), "one-slot pool initializes");
	d.pendingPolls = 100;
	Check(machine.Begin(Identity(1, 10)), "the only slot is accepted");
	Check(machine.End(), "the first sample is ended before the next begins");
	Check(!machine.Begin(Identity(2, 10)), "a full pool refuses a second sample");
	Check(machine.PoolFullCount() == 1, "pool-full is counted");
	Check(machine.Poll(13, 4) == 1 && output[0].status == obvr::perf::GpuStatus::Timeout,
	      "old pending data becomes a timeout without a flush");
	machine.Stop();
	machine.Shutdown();
}

void TestErrorsAndPartialAllocation() {
	FakeDevice d{};
	d.failCreateAfter = 1;
	obvr::perf::GpuSample output[4]{};
	obvr::perf::GpuQueryMachine partial;
	Check(!partial.Initialize(Ops(d), output, 4), "partial query allocation is refused");
	Check(d.released == 1, "a partially allocated pool is cleaned up");

	FakeDevice error{};
	error.failGet = true;
	obvr::perf::GpuQueryMachine machine;
	Check(machine.Initialize(Ops(error), output, 4), "error pool initializes");
	Check(machine.Begin(Identity(3, 4)) && machine.End(), "error sample is issued");
	Check(machine.Poll(5, 4) == 1 && output[0].status == obvr::perf::GpuStatus::QueryError,
	      "GetData errors invalidate only that sample");
	machine.Shutdown();
}
} // namespace

int main() {
	TestReadyAndPending();
	TestPoolAndTimeout();
	TestErrorsAndPartialAllocation();
	return failures == 0 ? 0 : 1;
}
