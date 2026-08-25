# Generates import libraries from the .def files, so that lld-link can bind
# against kernel32 and msvcrt without the Windows SDK.

set(OBVR_IMPORT_LIBS "")

foreach(module kernel32 msvcrt)
	set(def "${CMAKE_CURRENT_SOURCE_DIR}/${module}.def")
	set(lib "${CMAKE_CURRENT_BINARY_DIR}/${module}.lib")

	add_custom_command(
		OUTPUT "${lib}"
		COMMAND "${OBVR_DLLTOOL}" -m i386 -d "${def}" -l "${lib}" -k
		DEPENDS "${def}"
		COMMENT "Import library ${module}.lib"
		VERBATIM
	)

	list(APPEND OBVR_IMPORT_LIBS "${lib}")
endforeach()

add_custom_target(obvr_generate_imports DEPENDS ${OBVR_IMPORT_LIBS})

add_library(obvr_win32_imports INTERFACE)
add_dependencies(obvr_win32_imports obvr_generate_imports)
target_link_libraries(obvr_win32_imports INTERFACE ${OBVR_IMPORT_LIBS})
