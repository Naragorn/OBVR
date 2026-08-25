cmake_minimum_required(VERSION 3.20)

# Eigenstaendiges Projekt, bewusst nicht Teil des DLL-Baus.
#
# OBVR.dll wird fuer 32-Bit-Windows uebersetzt, die Tests laufen dagegen nativ
# auf dem Entwicklungsrechner. Beides in einem CMake-Lauf hiesse, zwei
# Toolchains gleichzeitig zu fahren. Die geprueften Bausteine sind reine
# Byteerzeugung und haengen nicht von der Zielarchitektur ab.
#
#   cmake -B build-tests tests
#   cmake --build build-tests
#   ctest --test-dir build-tests --output-on-failure

project(OBVRTests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_testing()

add_executable(trampoline_test
	TrampolineTest.cpp
	../src/camera/CameraTrampoline.cpp
	../src/core/CodeWriter.cpp
)

target_include_directories(trampoline_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(trampoline_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME trampoline COMMAND trampoline_test)

add_executable(quaternion_test
	QuaternionTest.cpp
	../src/vr/Quaternion.cpp
	../src/core/Rotation.cpp
)

target_include_directories(quaternion_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(quaternion_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME quaternion COMMAND quaternion_test)

add_executable(openvr_pose_test
	OpenVRPoseTest.cpp
	../src/vr/Quaternion.cpp
	../src/core/Rotation.cpp
)

target_include_directories(openvr_pose_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(openvr_pose_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME openvr_pose COMMAND openvr_pose_test)

# The backend test calls LoadLibrary and therefore only builds on Windows,
# unlike the other tests which are plain arithmetic. Keeping it behind WIN32
# preserves the native test run on Linux, where the rest still applies.
if(WIN32)
	add_executable(openvr_backend_test
		OpenVRBackendTest.cpp
		../src/vr/OpenVRBackend.cpp
		../src/vr/Quaternion.cpp
		../src/core/Log.cpp
	)

	target_include_directories(openvr_backend_test PRIVATE ../src)

	if(NOT MSVC)
		target_compile_options(openvr_backend_test PRIVATE -Wall -Wextra)
	endif()

	add_test(NAME openvr_backend COMMAND openvr_backend_test)
endif()
