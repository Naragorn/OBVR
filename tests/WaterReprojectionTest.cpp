#include <cmath>
#include <cstdio>
#include <limits>
#include <initializer_list>

#include "render/WaterReprojection.h"
#include "render/WaterReflectionBlend.h"

using namespace obvr;
using namespace obvr::render;

namespace {
unsigned failures = 0, checks = 0;
void Check(bool value, const char* message) {
	++checks;
	if (!value) { ++failures; std::printf("FAIL: %s\n", message); }
}
bool Near(float a, float b) { return std::fabs(a - b) < 0.0005f; }
bool NearMatrix(const WaterMatrix& a, const WaterMatrix& b) {
	for (unsigned row=0; row<4; ++row)
		for (unsigned col=0; col<4; ++col)
			if (!Near(a.m[row][col], b.m[row][col])) return false;
	return true;
}
WaterMatrix Identity() {
	WaterMatrix m{};
	for (unsigned i=0; i<4; ++i) m.m[i][i]=1.0f;
	return m;
}
NiTransform Camera(float radians, NiPoint3 position) {
	NiTransform t{};
	t.rot = NiMatrix33::Identity();
	t.rot.data[0][0]=std::cos(radians); t.rot.data[0][1]=-std::sin(radians);
	t.rot.data[1][0]=std::sin(radians); t.rot.data[1][1]=std::cos(radians);
	t.pos=position; t.scale=1.0f;
	return t;
}
}

int main() {
    for (unsigned inSubpass = 0; inSubpass < 2; ++inSubpass) {
        Check(ShouldBypassWaterReprojectionInReflectionSubpass(inSubpass != 0) == (inSubpass != 0),
              "reflection subpass bypass follows the nested-scene state");
    }
    for (unsigned rawMode = 0; rawMode < 4; ++rawMode) {
        const WaterReflectionMode mode = static_cast<WaterReflectionMode>(rawMode);
        const bool cyclopean = rawMode == 2 || rawMode == 3;
        const bool reprojected = rawMode == 2 || rawMode == 3;
        Check(UsesCyclopeanWaterCapture(mode) == cyclopean,
              "only modes 2 and 3 use the shared cyclopean capture");
        Check(UsesWaterReprojectionShader(mode) == reprojected,
              "modes 2 and 3 replace only the water reflection-coordinate shader");
        Check(UsesStableWaterReflectionShader(mode, false) == (rawMode == 2 || rawMode == 3),
              "stable capture modes use the world-space reflection shader");
        Check(!UsesStableWaterReflectionShader(mode, true),
              "coverage diagnostics always keep the diagnostic pixel shader");
        for (unsigned enabled = 0; enabled < 2; ++enabled)
        for (unsigned ready = 0; ready < 2; ++ready)
        for (unsigned camera = 0; camera < 2; ++camera)
        for (unsigned transforms = 0; transforms < 2; ++transforms) {
            const bool expected = enabled && cyclopean && (!reprojected || ready) &&
                                  camera && transforms;
            Check(CanUseStableWaterCapture(enabled != 0, mode, ready != 0,
                                           camera != 0, transforms != 0) == expected,
                  "all stable-capture gate combinations");
        }
    }

	{
		NiTransform local=Camera(0.6f,{1,2,3}); local.scale=1.25f;
		NiTransform world=Camera(-0.8f,{9,8,7});
		NiTransform parent=Camera(0.25f,{4,5,6}); parent.scale=2.0f;
		const NiMatrix33 base=Camera(-0.35f,{}).rot;
		NiTransform fixedLocal{},fixedWorld{};
		        const NiPoint3 basePosition{11,12,13};
        BuildHeadIndependentWaterCamera(local,world,parent,base,basePosition,fixedLocal,fixedWorld);
        Check(Near(fixedLocal.pos.x,basePosition.x) && Near(fixedLocal.pos.y,basePosition.y) &&
              Near(fixedLocal.pos.z,basePosition.z),"fixed capture removes the HMD and eye position offset");
        const NiPoint3 expectedWorldPos = parent.pos + parent.rot*(basePosition*parent.scale);
        Check(Near(fixedWorld.pos.x,expectedWorldPos.x) && Near(fixedWorld.pos.y,expectedWorldPos.y) &&
              Near(fixedWorld.pos.z,expectedWorldPos.z),"fixed capture composes the pre-HMD position under its parent");
		const NiMatrix33 expected=parent.rot*base;
		for(unsigned row=0;row<3;++row) for(unsigned col=0;col<3;++col) {
			Check(Near(fixedLocal.rot.data[row][col],base.data[row][col]),"fixed local rotation excludes HMD yaw");
			Check(Near(fixedWorld.rot.data[row][col],expected.data[row][col]),"fixed world rotation retains body and mouse heading");
		}
		Check(Near(fixedWorld.scale,parent.scale*local.scale),"fixed capture preserves composed scale");
	}
	{
	{
		NiTransform parent=Camera(0.25f,{4,5,6}); parent.scale=2.0f;
		NiTransform desired=Camera(-0.8f,{9,8,7}); desired.scale=1.5f;
		NiTransform local{};
		Check(BuildWaterCameraLocalFromParent(parent, desired, local),
		      "parent-relative water camera transform accepts finite parent and target");
		NiTransform reconstructed{};
		reconstructed.rot=parent.rot*local.rot;
		reconstructed.pos=parent.pos+parent.rot*(local.pos*parent.scale);
		reconstructed.scale=parent.scale*local.scale;
		for(unsigned row=0;row<3;++row) for(unsigned col=0;col<3;++col)
			Check(Near(reconstructed.rot.data[row][col],desired.rot.data[row][col]),
			      "parent-relative transform round-trips rotation");
		Check(Near(reconstructed.pos.x,desired.pos.x) && Near(reconstructed.pos.y,desired.pos.y) &&
		      Near(reconstructed.pos.z,desired.pos.z) && Near(reconstructed.scale,desired.scale),
		      "parent-relative transform round-trips position and scale");
		NiTransform invalid=parent; invalid.scale=0.0f;
		Check(!BuildWaterCameraLocalFromParent(invalid, desired, local),
		      "zero parent scale refuses a camera transform");
		desired.scale=std::numeric_limits<float>::quiet_NaN();
		Check(!BuildWaterCameraLocalFromParent(parent, desired, local),
		      "nonfinite desired scale refuses a camera transform");
	}

		const float values[]={0.0f,0.5f,1.0f,2.0f,16.0f,17.0f,
		 std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()};
		for(float horizontal:values) for(float vertical:values) {
			WaterMatrix matrix=Identity(),before=matrix;
			const bool valid=std::isfinite(horizontal)&&std::isfinite(vertical)&&
			 horizontal>=1&&horizontal<=16&&vertical>=1&&vertical<=16;
			Check(ScaleWaterProjectionRows(matrix,horizontal,vertical)==valid,
			      "all horizontal and vertical projection-scale validation flows");
			if(valid) Check(Near(matrix.m[0][0],1/horizontal)&&Near(matrix.m[1][1],1/vertical),
			                "valid scale widens both projection axes exactly");
			else Check(NearMatrix(matrix,before),"invalid projection scale leaves matrix untouched");
		}
	}


	{
		const NiTransform center=Camera(0.2f,{2,3,4});
		NiTransform left{},middle{},right{};
		BuildWaterReflectionCaptureTransform(center,-60.0f,left);
		BuildWaterReflectionCaptureTransform(center,0.0f,middle);
		BuildWaterReflectionCaptureTransform(center,60.0f,right);
		Check(Near(middle.pos.x,center.pos.x) && Near(middle.pos.y,center.pos.y) &&
		      Near(middle.pos.z,center.pos.z),"capture fan keeps the camera position");
		Check(NearMatrix(CameraWorldMatrix(middle),CameraWorldMatrix(center)),
		      "zero-yaw capture is the original body-fixed camera");
		Check(!NearMatrix(CameraWorldMatrix(left),CameraWorldMatrix(middle)) &&
		      !NearMatrix(CameraWorldMatrix(right),CameraWorldMatrix(middle)) &&
		      !NearMatrix(CameraWorldMatrix(left),CameraWorldMatrix(right)),
		      "left, center and right captures have distinct orientations");
		NiTransform tilted=Camera(0.2f,{2,3,4});
		NiTransform tiltedLeft{},tiltedRight{};
		tilted.rot=EulerToMatrix(30.0f,0.0f,20.0f);
		BuildWaterReflectionCaptureTransform(tilted,-60.0f,tiltedLeft);
		BuildWaterReflectionCaptureTransform(tilted,60.0f,tiltedRight);
		Check(Near(ForwardOf(tiltedLeft.rot).z,ForwardOf(tilted.rot).z) &&
		      Near(ForwardOf(tiltedRight.rot).z,ForwardOf(tilted.rot).z),
		      "world-yaw capture fan retains the centre camera pitch");
	}
	{
		Check(Near(WaterReflectionEdgeWeight(0.5f,0.5f,0.125f),1.0f),
		      "capture interior receives full weight");
		Check(Near(WaterReflectionEdgeWeight(0.0625f,0.5f,0.125f),0.5f),
		      "capture overlap fades linearly at an edge");
		const float outside[][2]={{-0.1f,0.5f},{1.1f,0.5f},{0.5f,-0.1f},{0.5f,1.1f}};
		for(const auto& uv:outside)
			Check(Near(WaterReflectionEdgeWeight(uv[0],uv[1],0.125f),0.0f),
			      "all four outside directions receive zero weight");
		const float bad[]={0.0f,-1.0f,std::numeric_limits<float>::infinity(),
		                   std::numeric_limits<float>::quiet_NaN()};
		for(float fade:bad)
			Check(Near(WaterReflectionEdgeWeight(0.5f,0.5f,fade),0.0f),
			      "invalid fade width refuses the sample");
		Check(Near(WaterReflectionEdgeWeight(std::numeric_limits<float>::quiet_NaN(),0.5f,0.125f),0.0f) &&
		      Near(WaterReflectionEdgeWeight(0.5f,std::numeric_limits<float>::infinity(),0.125f),0.0f),
		      "non-finite texture coordinates receive zero weight");
	}

    for (unsigned stable = 0; stable < 2; ++stable)
    for (unsigned rawPass = 0; rawPass < 3; ++rawPass) {
        const WaterStereoPass pass = static_cast<WaterStereoPass>(rawPass);
        Check(ShouldRenderWaterReflection(stable != 0, pass) ==
              (!stable || pass != WaterStereoPass::Second),
              "stable second eye alone reuses the first reflection texture");
    }
    for (unsigned rawMode = 0; rawMode < 4; ++rawMode)
    for (unsigned rawPass = 0; rawPass < 3; ++rawPass)
    for (unsigned valid = 0; valid < 2; ++valid) {
        const WaterReflectionMode mode = static_cast<WaterReflectionMode>(rawMode);
        const WaterStereoPass pass = static_cast<WaterStereoPass>(rawPass);
        WaterProjectionPlan expected = WaterProjectionPlan::ReprojectCurrent;
        if (mode == WaterReflectionMode::CyclopeanCapture) {
            expected = pass != WaterStereoPass::Second
                ? WaterProjectionPlan::CaptureAndStore
                : (valid ? WaterProjectionPlan::ReuseStored
                         : WaterProjectionPlan::MissingStored);
        }
        Check(ChooseWaterProjectionPlan(mode, pass, valid != 0) == expected,
              "all mode, eye-pass and stored-projection plan flows");
    }
	for (unsigned shadersReady = 0; shadersReady < 2; ++shadersReady)
	for (unsigned shadersRefused = 0; shadersRefused < 2; ++shadersRefused)
	for (unsigned hookReady = 0; hookReady < 2; ++hookReady) {
		const bool expected = shadersReady != 0 && shadersRefused == 0 && hookReady != 0;
		Check(CanUseWaterReprojection(shadersReady != 0, shadersRefused != 0,
		                              hookReady != 0) == expected,
		      "shader, refusal and capture-hook readiness gates all combinations" );
	}
	const float liveAngles[] = {0.0f, 0.35f, -0.7f};
	const float captureAngles[] = {0.0f, 0.2f, -0.5f};
	for (float liveAngle : liveAngles)
	for (float captureAngle : captureAngles) {
		const NiTransform live=Camera(liveAngle,{4,-3,7});
		const NiTransform capture=Camera(captureAngle,{4,-3,7});
		WaterMatrix world=Identity();
		world.m[0][0]=1.5f; world.m[1][1]=1.5f; world.m[2][2]=1.5f;
		world.m[0][3]=20; world.m[1][3]=-11; world.m[2][3]=3;
		WaterMatrix projection=Identity();
		projection.m[0][0]=1.3f; projection.m[1][1]=1.7f;
		projection.m[2][2]=1.01f; projection.m[2][3]=-0.2f;
		projection.m[3][2]=1.0f; projection.m[3][3]=0.0f;
		WaterMatrix inverseLive{}, inverseCapture{};
		Check(InvertWaterMatrix(CameraWorldMatrix(live),inverseLive),"live camera invertible");
		Check(InvertWaterMatrix(CameraWorldMatrix(capture),inverseCapture),"capture camera invertible");
		const WaterMatrix current=MultiplyWaterMatrix(
			MultiplyWaterMatrix(projection,inverseLive),world);
		const WaterMatrix expected=MultiplyWaterMatrix(
			MultiplyWaterMatrix(projection,inverseCapture),world);
		WaterMatrix actual{};
		Check(BuildReprojectedWaterMatrix(current,world,live,capture,actual),
		      "reprojection accepts affine world and camera transforms");
		Check(NearMatrix(actual,expected),
		      "reprojection reconstructs the exact capture-camera MVP");
	}
	{
		// A camera-relative WorldMat makes the old camera-only reuse path
		// change the full water MVP on the second eye. The stored full capture
		// MVP stays fixed and is therefore safe for the local-vertex shader.
		const NiTransform live=Camera(0.35f,{4,-3,7});
		const NiTransform capture=Camera(-0.5f,{4,-3,7});
		WaterMatrix world=Identity();
		world.m[0][3]=20.0f; world.m[1][3]=-11.0f; world.m[2][3]=3.0f;
		WaterMatrix projection=Identity();
		projection.m[0][0]=1.3f; projection.m[1][1]=1.7f;
		projection.m[2][2]=1.01f; projection.m[2][3]=-0.2f;
		projection.m[3][2]=1.0f; projection.m[3][3]=0.0f;
		WaterMatrix inverseLive{};
		Check(InvertWaterMatrix(CameraWorldMatrix(live),inverseLive),
		      "camera-relative reuse fixture has an invertible live camera");
		const WaterMatrix firstCurrent=MultiplyWaterMatrix(
			MultiplyWaterMatrix(projection,inverseLive),world);
		WaterMatrix stored{};
		Check(BuildStableWaterCaptureMvp(firstCurrent,world,live,capture,stored),
		      "stable capture stores a complete water MVP");
		WaterMatrix changedWorld=world;
		changedWorld.m[0][3]+=8.0f;
		changedWorld.m[1][3]-=5.0f;
		const WaterMatrix changedCurrent=MultiplyWaterMatrix(
			MultiplyWaterMatrix(projection,inverseLive),changedWorld);
		WaterMatrix recomposed{};
		Check(BuildStableWaterCaptureMvp(changedCurrent,changedWorld,live,capture,recomposed),
		      "changed camera-relative WorldMat remains mathematically valid");
		Check(!NearMatrix(stored,recomposed),
		      "rebuilding a full MVP from the later WorldMat changes the reflection coordinates");
		WaterMatrix reused{};
		Check(CopyStoredWaterCaptureMvp(stored,reused),
		      "stored full MVP can be copied for the second eye");
		Check(NearMatrix(stored,reused),
		      "second-eye reuse keeps the first-eye full MVP unchanged");
		WaterMatrix nonFinite=stored;
		nonFinite.m[0][0]=std::numeric_limits<float>::quiet_NaN();
		Check(!CopyStoredWaterCaptureMvp(nonFinite,reused),
		      "non-finite stored MVP refuses reuse");
		for (UInt32 capacity : {UInt32(0), UInt32(1), UInt32(2048)}) {
			for (UInt32 count : {UInt32(0), UInt32(1), UInt32(2047), UInt32(2048)}) {
				Check(CanStoreWaterCaptureMvp(count, capacity) ==
				          (capacity != 0 && count < capacity),
				      "all per-draw capture-MVP storage bounds");
			}
		}
		for (UInt32 count : {UInt32(0), UInt32(1), UInt32(2048)}) {
			for (UInt32 cursor : {UInt32(0), UInt32(1), UInt32(2047), UInt32(2048)}) {
				Check(CanReplayWaterCaptureMvp(cursor, count) == (cursor < count),
				      "all per-draw capture-MVP replay bounds");
			}
		}
	}
	{
		WaterMatrix projected=Identity(), world=Identity(), output{};
		Check(BuildWaterWorldProjectionMatrix(projected, world, output),
		      "world-space projection with identity world succeeds");
		Check(NearMatrix(projected, output),
		      "world-space projection preserves identity world result");
		world.m[0][0]=2.0f; world.m[1][1]=3.0f; world.m[2][2]=4.0f;
		world.m[0][3]=5.0f; world.m[1][3]=-6.0f; world.m[2][3]=7.0f;
		projected=world;
		Check(BuildWaterWorldProjectionMatrix(projected, world, output),
		      "world-space projection removes affine world transform");
		Check(NearMatrix(output, Identity()),
		      "world-space projection returns camera-only matrix");
		WaterMatrix singular=world; singular.m[3][3]=0.0f;
		Check(!BuildWaterWorldProjectionMatrix(projected, singular, output),
		      "world-space projection refuses singular world transform");
	}
	{
		const NiTransform camera=Camera(0.4f,{1,2,3});
		WaterMatrix world=Identity(), current=Identity(), output{};
		Check(BuildReprojectedWaterMatrix(current,world,camera,camera,output),
		      "matching live and capture camera accepted");
		Check(NearMatrix(current,output),
		      "matching cameras preserve the original MVP");
		world.m[3][3]=0;
		Check(!BuildReprojectedWaterMatrix(current,world,camera,camera,output),
		      "singular world matrix refuses reprojection");
		NiTransform singular=camera; singular.scale=0;
		world=Identity();
		Check(!BuildReprojectedWaterMatrix(current,world,camera,singular,output),
		      "singular capture camera refuses reprojection");
	}
	{
		const UInt32 original[] = {
			0x11111111,
			0x00000005,0x800F0000,0x80FF0001,0xA0E40003,
			0x00000004,0xE00F0002,0xA0E40000,0x80FF0001,0x80E40000,
			0x00000004,0xE00F0003,0xA0E40001,0x80FF0001,0x80E40000,
			0x00000004,0xE00F0004,0xA0E40002,0x80FF0001,0x80E40000,
			0x00000001,0xE00F0005,0xA0E40003,
			0x00000001,0xE00F0000,0x90E40000,
			0xFFFFFFFF,
		};
		UInt32 code[sizeof(original)/sizeof(original[0])];
		for(unsigned i=0;i<sizeof(code)/sizeof(code[0]);++i) code[i]=original[i];
		Check(PatchWaterVertexShader(code,sizeof(code)/sizeof(code[0])),
		      "exact five-instruction contract patches");
		unsigned found=0;
		for(UInt32 token:code)
			if(token==0xA0E4000D || token==0xA0E4000E ||
			   token==0xA0E4000F || token==0xA0E40010) ++found;
		Check(found==4,"diagnostic reflection rows use the stable c13-c16 matrix while oPos.w stays live");
		Check(code[4] == 0xA0E40003,"diagnostic shader keeps the live oPos.w source for perspective interpolation");
		Check(code[sizeof(code)/sizeof(code[0])-2] == 0x90E40000,
		      "diagnostic shader keeps the original local water position in oT0");
		Check(code[0]==original[0] && code[sizeof(code)/sizeof(code[0])-1]==0xFFFFFFFF,
		      "unrelated bytecode remains unchanged");
		UInt32 missing[sizeof(original)/sizeof(original[0])];
		for(unsigned i=0;i<sizeof(missing)/sizeof(missing[0]);++i) missing[i]=original[i];
		missing[2]=0;
		Check(!PatchWaterVertexShader(missing,sizeof(missing)/sizeof(missing[0])),
		      "missing instruction refuses shader");
		Check(!PatchWaterVertexShader(nullptr,0),"null bytecode refuses shader");
	}
	{
		const UInt32 nativeOriginal[] = {
			0x00000005,0x800F0000,0x80FF0001,0xA0E40003,
			0x00000004,0xE00F0002,0xA0E40000,0x80FF0001,0x80E40000,
			0x00000004,0xE00F0003,0xA0E40001,0x80FF0001,0x80E40000,
			0x00000004,0xE00F0004,0xA0E40002,0x80FF0001,0x80E40000,
			0x00000001,0xE00F0005,0xA0E40003,
			0x00000001,0xE00F0000,0x90E40000,
		};
		UInt32 code[sizeof(nativeOriginal)/sizeof(nativeOriginal[0])];
		for (unsigned i=0; i<sizeof(code)/sizeof(code[0]); ++i) code[i]=nativeOriginal[i];
		Check(PatchWaterVertexShaderNative(code,sizeof(code)/sizeof(code[0])),
		      "native shader patch finds reflection outputs and the true-world interpolant");
		unsigned found=0;
		for(UInt32 token:code)
			if(token==0xA0E4000D || token==0xA0E4000E ||
			   token==0xA0E4000F || token==0xA0E40010) ++found;
		Check(found==4,"native reflection rows use the stable c13-c16 matrix while oPos.w stays live");
		Check(code[3] == 0xA0E40003,"native shader keeps the live oPos.w source for perspective interpolation");
		Check(code[sizeof(code)/sizeof(code[0])-1] == 0x90E40000,
		      "native shader keeps the original local water position in oT0");
		UInt32 missing[sizeof(nativeOriginal)/sizeof(nativeOriginal[0])];
		for(unsigned i=0; i<sizeof(missing)/sizeof(missing[0]); ++i)
			missing[i]=nativeOriginal[i];
		missing[2]=0;
		Check(!PatchWaterVertexShaderNative(missing,sizeof(missing)/sizeof(missing[0])),
		      "missing native output refuses incomplete shader");
		Check(!PatchWaterVertexShaderNative(nullptr,0),"null native bytecode refuses shader");
	}
	std::printf("%u checks, %u failures\n",checks,failures);
	return failures?1:0;
}
