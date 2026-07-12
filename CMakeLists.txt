# CMake Project for FNA3D
# Written by @NeroBurner
cmake_minimum_required(VERSION 3.10)
project(FNA3D C)

# Options
option(BUILD_SHARED_LIBS "Build shared library" ON)

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
if(NOT MSVC)
	set_property(TARGET FNA3D PROPERTY COMPILE_FLAGS "-std=gnu99 -Wall -Wno-strict-aliasing -pedantic")
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
