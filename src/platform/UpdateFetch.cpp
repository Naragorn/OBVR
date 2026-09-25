#include "platform/UpdateFetch.h"

#include "core/AtomicFlag.h"
#include "core/Log.h"
#include "core/UpdateNotice.h"
#include "platform/Win32Min.h"

namespace obvr::platform {
namespace {

// winhttp.h (Windows SDK 10.0.26100.0), spelled out because the SDK-free
// build has no header for it and the DLL is loaded at run time anyway.
using HInternet = void*;
using WinHttpOpenFn = HInternet(__stdcall*)(const wchar_t* agent, DWORD accessType,
                                                const wchar_t* proxy, const wchar_t* bypass,
                                                DWORD flags);
using WinHttpConnectFn = HInternet(__stdcall*)(HInternet session, const wchar_t* server,
                                                   UInt16 port, DWORD reserved);
using WinHttpOpenRequestFn = HInternet(__stdcall*)(HInternet connect, const wchar_t* verb,
                                                       const wchar_t* object,
                                                       const wchar_t* version,
                                                       const wchar_t* referrer,
                                                       const wchar_t** acceptTypes, DWORD flags);
using WinHttpSendRequestFn = BOOL(__stdcall*)(HInternet request, const wchar_t* headers,
                                                  DWORD headersLength, void* optional,
                                                  DWORD optionalLength, DWORD totalLength,
                                                  UInt32 context);
using WinHttpReceiveResponseFn = BOOL(__stdcall*)(HInternet request, void* reserved);
using WinHttpQueryHeadersFn = BOOL(__stdcall*)(HInternet request, DWORD infoLevel,
                                                   const wchar_t* name, void* buffer,
                                                   DWORD* bufferLength, DWORD* index);
using WinHttpReadDataFn = BOOL(__stdcall*)(HInternet request, void* buffer, DWORD toRead,
                                               DWORD* read);
using WinHttpSetTimeoutsFn = BOOL(__stdcall*)(HInternet handle, int resolve, int connect,
                                                  int send, int receive);
using WinHttpCloseHandleFn = BOOL(__stdcall*)(HInternet handle);

constexpr DWORD kAccessTypeDefaultProxy = 0;
constexpr DWORD kAccessTypeAutomaticProxy = 4;  // Windows 8.1 and later
constexpr UInt16 kHttpsPort = 443;
constexpr DWORD kFlagSecure = 0x00800000;
constexpr DWORD kQueryStatusCode = 19;
constexpr DWORD kQueryFlagNumber = 0x20000000;

AtomicFlag g_started;
AtomicFlag g_done;
char g_tag[32] = {};
// The answer carries the release notes and the asset list after the tag;
// the tag sits near the top, so a cut-off answer still has it.
char g_body[32768];

DWORD __stdcall Fetch(void*) {
	const HMODULE module = LoadLibraryA("winhttp.dll");
	if (module == nullptr) {
		OBVR_LOG("Update check: winhttp.dll could not be loaded - no update notice");
		return 0;
	}
	const auto open = reinterpret_cast<WinHttpOpenFn>(GetProcAddress(module, "WinHttpOpen"));
	const auto connect =
		reinterpret_cast<WinHttpConnectFn>(GetProcAddress(module, "WinHttpConnect"));
	const auto openRequest =
		reinterpret_cast<WinHttpOpenRequestFn>(GetProcAddress(module, "WinHttpOpenRequest"));
	const auto send =
		reinterpret_cast<WinHttpSendRequestFn>(GetProcAddress(module, "WinHttpSendRequest"));
	const auto receive = reinterpret_cast<WinHttpReceiveResponseFn>(
		GetProcAddress(module, "WinHttpReceiveResponse"));
	const auto query =
		reinterpret_cast<WinHttpQueryHeadersFn>(GetProcAddress(module, "WinHttpQueryHeaders"));
	const auto read =
		reinterpret_cast<WinHttpReadDataFn>(GetProcAddress(module, "WinHttpReadData"));
	const auto timeouts =
		reinterpret_cast<WinHttpSetTimeoutsFn>(GetProcAddress(module, "WinHttpSetTimeouts"));
	const auto close =
		reinterpret_cast<WinHttpCloseHandleFn>(GetProcAddress(module, "WinHttpCloseHandle"));
	if (!open || !connect || !openRequest || !send || !receive || !query || !read ||
	    !timeouts || !close) {
		OBVR_LOG("Update check: winhttp.dll lacks an export - no update notice");
		return 0;
	}

	// GitHub rejects a request without a User-Agent; it asks for the
	// application's name there.
	const wchar_t* const agent = L"OBVR-update-check";
	HInternet session = open(agent, kAccessTypeAutomaticProxy, nullptr, nullptr, 0);
	if (session == nullptr) {
		session = open(agent, kAccessTypeDefaultProxy, nullptr, nullptr, 0);
	}
	if (session == nullptr) {
		OBVR_LOG("Update check: WinHttpOpen failed - no update notice");
		return 0;
	}
	timeouts(session, 5000, 5000, 5000, 5000);
	HInternet connection = connect(session, L"api.github.com", kHttpsPort, 0);
	HInternet request = connection == nullptr
	                        ? nullptr
	                        : openRequest(connection, L"GET",
	                                      L"/repos/Naragorn/OBVR/releases/latest", nullptr,
	                                      nullptr, nullptr, kFlagSecure);
	DWORD status = 0;
	UInt32 length = 0;
	if (request != nullptr &&
	    send(request, L"Accept: application/vnd.github+json\r\n", static_cast<DWORD>(-1),
	         nullptr, 0, 0, 0) &&
	    receive(request, nullptr)) {
		DWORD size = sizeof(status);
		query(request, kQueryStatusCode | kQueryFlagNumber, nullptr, &status, &size, nullptr);
		if (status == 200) {
			DWORD got = 0;
			while (length + 1 < sizeof(g_body) &&
			       read(request, g_body + length, sizeof(g_body) - 1 - length, &got) && got > 0) {
				length += got;
			}
		}
	}
	g_body[length] = '\0';
	if (request != nullptr) {
		close(request);
	}
	if (connection != nullptr) {
		close(connection);
	}
	close(session);

	char tag[sizeof(g_tag)];
	if (status != 200) {
		OBVR_LOG("Update check: no answer from GitHub (HTTP status %u) - no update notice",
		         static_cast<unsigned>(status));
	} else if (!update::ExtractTagName(g_body, length, tag, sizeof(tag))) {
		OBVR_LOG("Update check: the answer (%u bytes) carried no readable tag_name", length);
	} else {
		for (UInt32 i = 0; i < sizeof(tag); ++i) {
			g_tag[i] = tag[i];
		}
		g_done.Set(true);
		OBVR_LOG("Update check: latest release %s, running %s - %s", g_tag, OBVR_VERSION_STRING,
		         update::IsNewerVersion(g_tag, OBVR_VERSION_STRING) ? "an update is available"
		                                                            : "up to date");
	}
	return 0;
}

}  // namespace

void StartUpdateCheck() {
	if (g_started.Get()) {
		return;
	}
	g_started.Set(true);
	HANDLE thread = CreateThread(nullptr, 0, &Fetch, nullptr, 0, nullptr);
	if (thread == nullptr) {
		OBVR_LOG("Update check: the thread could not be started - no update notice");
		return;
	}
	CloseHandle(thread);
}

bool LatestReleaseTag(char* out, UInt32 capacity) {
	if (!g_done.Get() || out == nullptr || capacity == 0) {
		return false;
	}
	UInt32 i = 0;
	for (; g_tag[i] != '\0' && i + 1 < capacity; ++i) {
		out[i] = g_tag[i];
	}
	out[i] = '\0';
	return g_tag[i] == '\0';
}

}  // namespace obvr::platform
