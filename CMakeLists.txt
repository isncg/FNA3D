# CMake Project for FNA3D
# Written by @NeroBurner
cmake_minimum_required(VERSION 3.10)
project(FNA3D C)

# Options
option(BUILD_SHARED_LIBS "Build shared library" ON)
option(FNA3D_IMGUI "Build the Dear ImGui integration (dcimgui) for C# P/Invoke" ON)

# Version
SET(LIB_MAJOR_VERSION "27")
SET(LIB_MINOR_VERSION "0")
SET(LIB_REVISION "0")
SET(LIB_VERSION "${LIB_MAJOR_VERSION}.${LIB_MINOR_VERSION}.${LIB_REVISION}")

# Build Type
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	# By default, we use Release
	message(STATUS "Setting build type to 'Release' as none was specified.")
	set(CMAKE_BUILD_TYPE "Release" CACHE
		STRING "Choose the type of build." FORCE
	)

	# Set the possible values of build type for cmake-gui
	set_property(CACHE CMAKE_BUILD_TYPE PROPERTY
		STRINGS "Debug" "Release" "RelWithDebInfo"
	)
endif()

# Platform Flags
if(APPLE)
	set(CMAKE_MACOSX_RPATH ON)
	set(CMAKE_OSX_DEPLOYMENT_TARGET 10.9)
	set(LOBJC "objc")
elseif(WIN32)
	# "FNA3D.dll", not "libFNA3D.dll"
	set(CMAKE_SHARED_LIBRARY_PREFIX "")
endif()

# Defines
add_definitions(
	-DFNA3D_DRIVER_SDL
	-DUSE_SDL3
)

if(EMSCRIPTEN)
	# No special defines needed for SDL GPU on WASM
endif()

# Source lists
add_library(FNA3D
	# Public Headers
	include/FNA3D.h
	include/FNA3D_Image.h
	# Internal Headers
	src/FNA3D_Driver.h
	src/FNA3D_Effect.h
	src/FNA3D_PipelineCache.h
	# Source Files
	src/FNA3D.c
	src/FNA3D_Driver_SDL.c
	src/FNA3D_Effect.c
	src/FNA3D_Image.c
	src/FNA3D_PipelineCache.c
)

# Build flags
# NOTE: keep these C-only so the optional C++ Dear ImGui sources (added below
# when FNA3D_IMGUI is enabled) are not compiled with -std=gnu99.
if(NOT MSVC)
	target_compile_options(FNA3D PRIVATE
		$<$<COMPILE_LANGUAGE:C>:-std=gnu99 -Wall -Wno-strict-aliasing -pedantic>
	)
endif()

# FNA3D folders as includes, for other targets to consume
target_include_directories(FNA3D PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

# MinGW builds should statically link libgcc
if(MINGW)
	target_link_libraries(FNA3D PRIVATE -static-libgcc)
endif()

# Soname
set_target_properties(FNA3D PROPERTIES OUTPUT_NAME "FNA3D"
	VERSION ${LIB_VERSION}
	SOVERSION ${LIB_MAJOR_VERSION}
)

# Internal Dependencies
target_link_libraries(FNA3D PRIVATE ${LOBJC})

# SDL Dependency
if (DEFINED SDL3_INCLUDE_DIRS AND DEFINED SDL3_LIBRARIES)
	message(STATUS "using pre-defined SDL3 variables SDL3_INCLUDE_DIRS and SDL3_LIBRARIES")
	target_include_directories(FNA3D PUBLIC "$<BUILD_INTERFACE:${SDL3_INCLUDE_DIRS}>")
	target_link_libraries(FNA3D PUBLIC ${SDL3_LIBRARIES})
else()
	# Only try to autodetect if both SDL3 variables aren't explicitly set
	find_package(SDL3 CONFIG)
	if (TARGET SDL3::SDL3)
		message(STATUS "using TARGET SDL3::SDL3")
		target_link_libraries(FNA3D PUBLIC SDL3::SDL3)
	elseif (TARGET SDL3)
		message(STATUS "using TARGET SDL3")
		target_link_libraries(FNA3D PUBLIC SDL3)
	else()
		message(STATUS "no TARGET SDL3::SDL3, or SDL3, using variables")
		target_include_directories(FNA3D PUBLIC "$<BUILD_INTERFACE:${SDL3_INCLUDE_DIRS}>")
		target_link_libraries(FNA3D PUBLIC ${SDL3_LIBRARIES})
	endif()
endif()

# Dear ImGui integration (optional)
#
# Compiles Dear ImGui (C++), its SDL3 + SDL_GPU backends, the generated dcimgui
# flat C bindings, and the FNA3D<->ImGui glue directly into the FNA3D library.
# dcimgui's C ABI is exported so C# can P/Invoke the full ImGui widget API; the
# backends are driven internally by FNA3D and never exposed to the host.
if(FNA3D_IMGUI)
	enable_language(CXX)
	find_package(Python3 REQUIRED COMPONENTS Interpreter)

	set(FNA3D_IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/imgui)
	set(DEAR_BINDINGS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/dear_bindings)
	set(DCIMGUI_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)

	# Apply tracked patches to the vendored imgui submodule at configure time.
	# These live in patches/ (rather than a submodule fork) so they survive
	# `git submodule update`. Re-application is idempotent: `git apply --reverse
	# --check` succeeds only when the patch is already present, so we skip it.
	find_package(Git QUIET)
	set(FNA3D_IMGUI_PATCHES
		${CMAKE_CURRENT_SOURCE_DIR}/patches/0001-sdlgpu3-enable-depth-clip.patch
	)
	if(Git_FOUND)
		foreach(_patch ${FNA3D_IMGUI_PATCHES})
			get_filename_component(_patch_name ${_patch} NAME)
			execute_process(
				COMMAND ${GIT_EXECUTABLE} apply --reverse --check ${_patch}
				WORKING_DIRECTORY ${FNA3D_IMGUI_DIR}
				RESULT_VARIABLE _patch_applied
				OUTPUT_QUIET ERROR_QUIET
			)
			if(NOT _patch_applied EQUAL 0)
				execute_process(
					COMMAND ${GIT_EXECUTABLE} apply ${_patch}
					WORKING_DIRECTORY ${FNA3D_IMGUI_DIR}
					RESULT_VARIABLE _patch_result
				)
				if(NOT _patch_result EQUAL 0)
					message(FATAL_ERROR "Failed to apply imgui patch: ${_patch_name}")
				endif()
				message(STATUS "Applied imgui patch: ${_patch_name}")
			else()
				message(STATUS "imgui patch already applied: ${_patch_name}")
			endif()
		endforeach()
	else()
		message(WARNING "git not found; cannot apply imgui patches in patches/ "
			"(ImGui may assert on enable_depth_clip with FNA3D devices)")
	endif()

	# Generate the flat C bindings (dcimgui.{h,cpp,json}) from imgui.h.
	add_custom_command(
		OUTPUT
			${DCIMGUI_GEN_DIR}/dcimgui.cpp
			${DCIMGUI_GEN_DIR}/dcimgui.h
			${DCIMGUI_GEN_DIR}/dcimgui.json
		COMMAND ${CMAKE_COMMAND} -E make_directory ${DCIMGUI_GEN_DIR}
		COMMAND ${Python3_EXECUTABLE} ${DEAR_BINDINGS_DIR}/dear_bindings.py
			-o ${DCIMGUI_GEN_DIR}/dcimgui
			${FNA3D_IMGUI_DIR}/imgui.h
		DEPENDS ${FNA3D_IMGUI_DIR}/imgui.h ${DEAR_BINDINGS_DIR}/dear_bindings.py
		COMMENT "Generating dcimgui C bindings (dear_bindings)"
		VERBATIM
	)

	target_sources(FNA3D PRIVATE
		${FNA3D_IMGUI_DIR}/imgui.cpp
		${FNA3D_IMGUI_DIR}/imgui_draw.cpp
		${FNA3D_IMGUI_DIR}/imgui_tables.cpp
		${FNA3D_IMGUI_DIR}/imgui_widgets.cpp
		${FNA3D_IMGUI_DIR}/imgui_demo.cpp
		${FNA3D_IMGUI_DIR}/backends/imgui_impl_sdl3.cpp
		${FNA3D_IMGUI_DIR}/backends/imgui_impl_sdlgpu3.cpp
		${DCIMGUI_GEN_DIR}/dcimgui.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/src/FNA3D_ImGui.cpp
	)
	# C++17 applies only to the C++ sources; C sources keep their gnu99 flags.
	target_compile_features(FNA3D PRIVATE cxx_std_17)
	# backends/ is only needed to build FNA3D's own glue.
	target_include_directories(FNA3D PRIVATE ${FNA3D_IMGUI_DIR}/backends)
	# Expose imgui.h/imconfig.h and the generated dcimgui.h to consumers, so C
	# (and C++) callers linking FNA3D can #include "dcimgui.h" for the ImGui
	# widget API without knowing where the generated files live.
	target_include_directories(FNA3D PUBLIC
		$<BUILD_INTERFACE:${FNA3D_IMGUI_DIR}>
		$<BUILD_INTERFACE:${DCIMGUI_GEN_DIR}>
	)
	# FNA3D_IMGUI enables the driver-side hooks; CIMGUI_API exports the dcimgui
	# C symbols from the shared library (default visibility already exports on
	# ELF/Mach-O, so this only matters on Windows).
	target_compile_definitions(FNA3D PRIVATE FNA3D_IMGUI)
	if(WIN32)
		target_compile_definitions(FNA3D PRIVATE "CIMGUI_API=__declspec(dllexport)")
	endif()

# 	# Copy generated dcimgui.h to include/ so consumers (FNA3D_HLSL_Test, C#)
# 	# can #include "dcimgui.h" without knowing the build-tree path.
# 	add_custom_command(TARGET FNA3D POST_BUILD
# 		COMMAND ${CMAKE_COMMAND} -E copy_if_different
# 			"${DCIMGUI_GEN_DIR}/dcimgui.h"
# 			"${CMAKE_CURRENT_SOURCE_DIR}/include/dcimgui.h"
# 		COMMENT "Copying dcimgui.h to include/"
# 	)

		# ImGuizmo (3D gizmo/manipulator extension for ImGui)
		option(FNA3D_IMGUIZMO "Build the ImGuizmo integration" ON)
		if(FNA3D_IMGUIZMO)
			set(FNA3D_IMGUIZMO_DIR ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/imguizmo/src)
			target_sources(FNA3D PRIVATE
				${FNA3D_IMGUIZMO_DIR}/ImGuizmo.cpp
				${CMAKE_CURRENT_SOURCE_DIR}/src/FNA3D_ImGuizmo.cpp
			)
			target_include_directories(FNA3D PRIVATE
				${FNA3D_IMGUIZMO_DIR}
			)
			target_compile_definitions(FNA3D PRIVATE FNA3D_IMGUIZMO)
		endif()
	endif()

# define install directories
# on mingw-w64 cross compilation $CMAKE_INSTALL_LIBDIR is set to an absolute
# path. Work around that by hard coding the directories on windows
if(WIN32)
	set(FNA3D_INSTALL_INCLUDEDIR include)
	set(FNA3D_INSTALL_BINDIR bin)
	set(FNA3D_INSTALL_LIBDIR lib)
else()
	include(GNUInstallDirs)
	set(FNA3D_INSTALL_INCLUDEDIR ${CMAKE_INSTALL_INCLUDEDIR})
	set(FNA3D_INSTALL_BINDIR ${CMAKE_INSTALL_BINDIR})
	set(FNA3D_INSTALL_LIBDIR ${CMAKE_INSTALL_LIBDIR})
endif()

# Installation (Shared only!)

if(BUILD_SHARED_LIBS)
	# Public Headers...
	install(
		DIRECTORY include/
		DESTINATION ${FNA3D_INSTALL_INCLUDEDIR}
	)
	# Libraries...
	install(
		TARGETS ${PROJECT_NAME}
		EXPORT ${PROJECT_NAME}-targets
		INCLUDES DESTINATION ${FNA3D_INSTALL_INCLUDEDIR}
		RUNTIME DESTINATION ${FNA3D_INSTALL_BINDIR}
		LIBRARY DESTINATION ${FNA3D_INSTALL_LIBDIR}
		ARCHIVE DESTINATION ${FNA3D_INSTALL_LIBDIR}
	)

	# Generate a pkgconfig file
	include(cmake/JoinPaths.cmake)
	join_paths(FNA3D_PKGCONF_LIBDIR "\${prefix}" "${CMAKE_INSTALL_LIBDIR}")
	join_paths(FNA3D_PKGCONF_INCLUDEDIR "\${prefix}" "${CMAKE_INSTALL_INCLUDEDIR}")

	if(NOT PLATFORM_WIN32)
		set(PC_REQUIRES_PRIVATE "Requires.private: sdl3")
	endif()

	configure_file(
		"${PROJECT_SOURCE_DIR}/cmake/${PROJECT_NAME}.pc.in"
		${PROJECT_BINARY_DIR}/generated/${PROJECT_NAME}.pc
		@ONLY
	)
	install(
		FILES ${CMAKE_CURRENT_BINARY_DIR}/generated/${PROJECT_NAME}.pc
		DESTINATION ${FNA3D_INSTALL_LIBDIR}/pkgconfig
	)

	# Generate cmake-config file, install CMake files
	include(CMakePackageConfigHelpers)
	configure_package_config_file(
		cmake/config.cmake.in
		${CMAKE_CURRENT_BINARY_DIR}/generated/${PROJECT_NAME}Config.cmake
		INSTALL_DESTINATION ${FNA3D_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
	)
	install(
		FILES ${CMAKE_CURRENT_BINARY_DIR}/generated/${PROJECT_NAME}Config.cmake
		DESTINATION ${FNA3D_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
	)

	install(
		EXPORT ${PROJECT_NAME}-targets
		NAMESPACE ${PROJECT_NAME}::
		DESTINATION ${FNA3D_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
	)
endif()

# Ship dcimgui.json so consumers can generate matching C# P/Invoke bindings.
if(FNA3D_IMGUI AND BUILD_SHARED_LIBS)
	install(
		FILES ${DCIMGUI_GEN_DIR}/dcimgui.json
		DESTINATION ${FNA3D_INSTALL_INCLUDEDIR}
	)
endif()
