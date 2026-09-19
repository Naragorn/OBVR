#include <cstdio>
#include <cmath>
#include <limits>
#include "render/LeafFacing.h"
#include "render/LeafHookPlan.h"
#include "core/AroundCall.h"

using namespace obvr;
using namespace obvr::render;
static unsigned failures = 0, checks = 0;
void Check(bool ok, const char* message) {
    ++checks;
    if (!ok) { ++failures; std::printf("FAIL: %s\n", message); }
}
bool Near(float a, float b) { return std::fabs(a-b) < 0.00001f; }
int main() {
    // Every partial-install state, signature-failure combination and write
    // failure position. Failure must leave correction disabled; retries must
    // finish without modifying any site that already belongs to us.
    for (unsigned initial=0; initial<8; ++initial)
    for (unsigned bad=0; bad<8; ++bad)
    for (unsigned failWrite=0; failWrite<4; ++failWrite) {
        unsigned state=initial, verified=0, written=0;
        bool failedVerification = (bad & (~initial & 7)) != 0;
        bool ok=InstallLeafSites(state, [&](unsigned i) {
            Check(!(initial & (1u<<i)), "owned site never verified as vanilla");
            verified |= 1u<<i; return !(bad & (1u<<i));
        }, [&](unsigned i) {
            Check(verified == (~initial & 7), "all signatures checked before mutation");
            Check(!(initial & (1u<<i)), "owned site never rewritten");
            if (i==failWrite) return false;
            written |= 1u<<i; return true;
        });
        Check(state==(initial|written), "state records only successful writes");
        Check(ok == (!failedVerification && (failWrite==3 || (initial & (1u<<failWrite)))),
              "all refusal and partial write flows");
        if (failedVerification) Check(written==0, "signature mismatch writes nothing");
        unsigned retryWrites=0;
        const unsigned beforeRetry=state;
        Check(InstallLeafSites(state, [](unsigned){return true;}, [&](unsigned i){
            retryWrites|=1u<<i; return true;
        }), "retry completes");
        Check(state==7 && retryWrites==(~beforeRetry&7), "retry writes exactly remaining sites");
    }
    const UInt8 expected[3][5] = {
        {0xE8,0x9C,0xFA,0xFF,0xFF},
        {0xE8,0x6A,0x80,0xFF,0xFF},
        {0xE8,0x55,0x7F,0xFF,0xFF}
    };
    for(unsigned i=0;i<3;++i) {
        UInt8 actual[5]{};
        Check(mem::BuildCallSitePatch(actual,5,kLeafHookSites[i].address,kLeafHookSites[i].original)==5,
              "call patch size");
        for(unsigned b=0;b<5;++b) Check(actual[b]==expected[i][b],"verified executable call signature");
    }
    for(unsigned gates=0;gates<8;++gates) {
        NiPoint3 r{7,8,9},u{4,5,6};
        bool ok=LeafFacing(gates&1,gates&2,gates&4,{0,0,0},{0,10,0},r,u);
        Check(ok==(gates==7),"all enable/world/camera combinations");
        if(!ok) Check(r.x==7&&u.x==4,"refusal preserves original basis");
    }
    // The first tree is east (edge-on when looking north), later trees are
    // north/west/south. Removing or reordering that first tree cannot alter
    // any remaining tree's basis. Reuse the SAME transform storage, as the
    // engine's optimized batch does, to catch address-keyed caching.
    const NiPoint3 trees[]={{10,0,0},{0,10,0},{-10,0,0},{0,-10,0}};
    const NiPoint3 expectedRight[]={{0,-1,0},{1,0,0},{0,1,0},{-1,0,0}};
    NiTransform reused{};
    for(unsigned first=0;first<4;++first)
    for(unsigned eye=0;eye<2;++eye)
    for(unsigned j=0;j<4;++j) {
        unsigned i=(first+j)%4;
        reused.pos=trees[i];
        NiPoint3 r{},u{};
        Check(LeafFacing(true,true,true,{0,0,0},reused.pos,r,u),"each batch tree accepted");
        Check(Near(r.x,expectedRight[i].x)&&Near(r.y,expectedRight[i].y)&&u.z==1,
              "basis belongs to current tree, independent of order and eye");
    }
    // Crossing behind a card must flip its front face. Both rendered eyes use
    // the same midpoint, so their outputs remain identical.
    {
        NiPoint3 frontRight{}, frontUp{}, backRight{}, backUp{};
        NiPoint3 leftRight{}, leftUp{}, rightRight{}, rightUp{};
        Check(LeafFacing(true,true,true,{0,0,0},{0,10,0},frontRight,frontUp),
              "front side accepted");
        Check(LeafFacing(true,true,true,{0,20,0},{0,10,0},backRight,backUp),
              "back side accepted");
        Check(Near(frontRight.x,-backRight.x)&&Near(frontRight.y,-backRight.y),
              "crossing behind a leaf flips its front face toward the camera");
        const NiPoint3 midpoint{4,3,2};
        Check(LeafFacing(true,true,true,midpoint,{10,10,0},leftRight,leftUp) &&
              LeafFacing(true,true,true,midpoint,{10,10,0},rightRight,rightUp),
              "both eyes accept their shared midpoint");
        Check(Near(leftRight.x,rightRight.x)&&Near(leftRight.y,rightRight.y)&&
              Near(leftUp.z,rightUp.z),
              "both eyes receive one cyclopean leaf basis");
    }
    const float bad[] = {0,0.001f,1.0e20f,std::numeric_limits<float>::infinity(),
                         std::numeric_limits<float>::quiet_NaN()};
    for(float x:bad) {
        NiPoint3 r{7,8,9},u{4,5,6};
        Check(!LeafFacing(true,true,true,{0,0,0},{x,0,0},r,u)&&r.x==7&&u.x==4,
              "degenerate/nonfinite input preserves fallback");
    }
    std::printf("%u checks, %u failures\n",checks,failures);
    return failures?1:0;
}
