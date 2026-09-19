#include "render/LeafFacingHook.h"
#include "render/LeafFacing.h"
#include "render/LeafAudit.h"
#include "render/LeafHookPlan.h"
#include "camera/CameraHook.h"
#include "core/AroundCall.h"
#include "core/AddressSpace.h"
#include "core/Config.h"
#include "core/Log.h"
#include "core/Memory.h"
#include "game/GameAddresses.h"
#include "game/PlayerAim.h"

namespace obvr::render {
namespace {
// Oblivion 1.2.0.416; see docs/leaf-batch-fix.md for the disassembly.
// The initial setup calls 7F1170, copying camera UP/RIGHT columns into
// B46768/B46758. The optimized leaf loop skips that setup on later trees.
void (__cdecl* g_original)() = reinterpret_cast<void(__cdecl*)()>(0x007F1170);
bool g_installed = false;
unsigned g_installedSites = 0;
LeafAudit g_audit;
LeafAudit g_batchAudit;

void ApplyLeafFacing(const NiTransform* transform, UInt32 geometry = 0) {
    const auto& config = GetConfig();
    if (!g_installed || !config.stableLeafBillboards ||
        config.tracker.source != vr::TrackerSource::OpenVR ||
        !game::PlayerInWorld() || transform == nullptr) return;
    const UInt32 scene = *reinterpret_cast<const UInt32*>(addr::kWorldSceneGraphPointer);
    if (!mem::LooksLikeObjectAddress(scene)) return;
    const UInt32 mainCamera = *reinterpret_cast<const UInt32*>(scene + addr::kSceneGraphCameraOffset);
    const UInt32 currentCamera = *reinterpret_cast<const UInt32*>(0x00B43124);
    if (currentCamera == mainCamera) {
        g_audit.Record(reinterpret_cast<UInt32>(transform));
        g_batchAudit.Record(geometry);
    }
    NiPoint3 cameraWorld{};
    if (!camera::GetCyclopeanCameraWorldPosition(cameraWorld)) return;
    NiPoint3 right{}, up{};
    if (!LeafFacing(true, true, mainCamera != 0 && currentCamera == mainCamera,
                    cameraWorld, transform->pos, right, up)) return;
    *reinterpret_cast<NiPoint3*>(0x00B46758) = right;
    *reinterpret_cast<NiPoint3*>(0x00B46768) = up;
    static bool reported = false;
    if (!reported) {
        reported = true;
        OBVR_LOG("Leaves: per-tree cyclopean-camera billboard basis applied to main camera %08X", mainCamera);
    }
}

void __cdecl ApplyInitialLeafFacing(const NiTransform* transform) {
    ApplyLeafFacing(transform);
}

__declspec(naked) void LeafCall() {
    __asm {
        call dword ptr [g_original]
        pushfd
        pushad
        push esi
        call ApplyInitialLeafFacing
        add esp, 4
        popad
        popfd
        ret
    }
}

// 7F8B51 and 7F8C66 both call 7F0BC0 with seven stack arguments.
// Argument six is the CURRENT tree's copied world transform. These paths
// skip 7F15E0/7F16CF, then upload the constants in 7F6BF0 before drawing.
using BatchSetup = UInt32(__fastcall*)(void*, void*, UInt32, UInt32, UInt32,
                                      UInt32, UInt32, const NiTransform*, UInt32);
UInt32 __fastcall LeafBatchCall(void* self, void* unusedEdx, UInt32 geometry,
                               UInt32 a2, UInt32 a3, UInt32 a4, UInt32 a5,
                               const NiTransform* transform, UInt32 a7) {
    const UInt32 result = reinterpret_cast<BatchSetup>(0x007F0BC0)(
        self, unusedEdx, geometry, a2, a3, a4, a5, transform, a7);
    if (g_installed) {
        // Disabled/refused/secondary-camera calls must never inherit another
        // tree's corrected basis. This also resets both fourth components.
        g_original();
        ApplyLeafFacing(transform, geometry);
    }
    return result;
}
}

void BeginLeafAudit(bool second) {
    if (second) { g_audit.Second(); g_batchAudit.Second(); }
    else { g_audit.Begin(); g_batchAudit.Begin(); }
}
void EndLeafAudit(UInt32 frame) {
    const auto r = g_audit.End();
    const auto b = g_batchAudit.End();
    static unsigned reports = 0;
    if (frame % 120 == 0 || ((r.leftOnly || r.rightOnly || r.overflow) && reports++ < 12))
        OBVR_LOG("Leaves audit frame=%u leftOnly=%u rightOnly=%u both=%u overflow=%u callsFirst=%u callsSecond=%u",
                 frame,r.leftOnly,r.rightOnly,r.both,r.overflow,r.callsFirst,r.callsSecond);
    if (frame % 120 == 0)
        OBVR_LOG("Leaves batch frame=%u leftOnly=%u rightOnly=%u both=%u overflow=%u callsFirst=%u callsSecond=%u",
                 frame,b.leftOnly,b.rightOnly,b.both,b.overflow,b.callsFirst,b.callsSecond);
}
bool InstallLeafFacingHook() {
    if (g_installed) return true;
    g_installed = InstallLeafSites(g_installedSites, [](unsigned i) {
        const auto site = kLeafHookSites[i];
        UInt8 expected[5];
        mem::BuildCallSitePatch(expected, sizeof(expected), site.address, site.original);
        if (mem::Verify(site.address, expected, sizeof(expected))) return true;
        OBVR_LOG("Leaves: call at %08X differs; stable basis disabled", site.address);
        mem::ReportForeignCode("Leaves", site.address);
        return false;
    }, [](unsigned i) {
        UInt8 patch[5];
        const UInt32 target = i == 0 ? reinterpret_cast<UInt32>(&LeafCall)
                                    : reinterpret_cast<UInt32>(&LeafBatchCall);
        mem::BuildCallSitePatch(patch, sizeof(patch), kLeafHookSites[i].address, target);
        return mem::SafeWrite(kLeafHookSites[i].address, patch, sizeof(patch));
    });
    OBVR_LOG("Leaves: per-tree setup hooks %s (sites=%u)",
             g_installed ? "installed" : "inactive", g_installedSites);
    return g_installed;
}
}
