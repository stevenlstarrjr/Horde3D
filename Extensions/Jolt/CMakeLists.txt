# Jolt Physics integration for Horde3D.
option(HORDE3D_BUILD_JOLT "Build the Jolt Physics extension into Horde3D" OFF)
option(HORDE3D_JOLT_FETCH "Download Jolt Physics when no installed package is available" ON)
set(HORDE3D_JOLT_VERSION "v5.6.0" CACHE STRING "Jolt Physics release used by FetchContent")

if(HORDE3D_BUILD_JOLT)
	find_package(Jolt CONFIG QUIET)

	if(TARGET Jolt::Jolt)
		set(HORDE3D_JOLT_TARGET Jolt::Jolt)
	elseif(TARGET Jolt)
		set(HORDE3D_JOLT_TARGET Jolt)
	elseif(HORDE3D_JOLT_FETCH)
		if(CMAKE_VERSION VERSION_LESS 3.20)
			message(FATAL_ERROR "Jolt Physics ${HORDE3D_JOLT_VERSION} requires CMake 3.20 or newer")
		endif()

		include(FetchContent)
		set(OVERRIDE_CXX_FLAGS OFF CACHE BOOL "" FORCE)
		set(INTERPROCEDURAL_OPTIMIZATION OFF CACHE BOOL "" FORCE)
		set(JPH_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
		set(ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
		set(ENABLE_ALL_WARNINGS OFF CACHE BOOL "" FORCE)
		set(DEBUG_RENDERER_IN_DEBUG_AND_RELEASE OFF CACHE BOOL "" FORCE)
		set(PROFILER_IN_DEBUG_AND_RELEASE OFF CACHE BOOL "" FORCE)
		set(JPH_USE_DX12 OFF CACHE BOOL "" FORCE)
		set(JPH_USE_VK OFF CACHE BOOL "" FORCE)
		set(JPH_USE_MTL OFF CACHE BOOL "" FORCE)
		set(JPH_USE_CPU_COMPUTE OFF CACHE BOOL "" FORCE)
		set(CMAKE_POSITION_INDEPENDENT_CODE ON)

		FetchContent_Declare(JoltPhysics
			GIT_REPOSITORY https://github.com/jrouwe/JoltPhysics.git
			GIT_TAG ${HORDE3D_JOLT_VERSION}
			GIT_SHALLOW TRUE
			SOURCE_SUBDIR Build
		)
		FetchContent_MakeAvailable(JoltPhysics)
		set(HORDE3D_JOLT_TARGET Jolt)
	else()
		message(FATAL_ERROR "HORDE3D_BUILD_JOLT is ON, but Jolt Physics was not found and fetching is disabled")
	endif()

	add_subdirectory(Bindings)
	add_subdirectory(Source)
	if(HORDE3D_BUILD_EXAMPLES)
		add_subdirectory(Sample)
	endif()

	set(HORDE3D_EXTENSION_INSTALLS "${HORDE3D_EXTENSION_INSTALLS}extMan().installExtension( new Horde3DJolt::ExtJolt() );\n" PARENT_SCOPE)
	set(HORDE3D_EXTENSION_INCLUDES "${HORDE3D_EXTENSION_INCLUDES}\n#include \"Extensions/Jolt/Source/extension.h\"\n" PARENT_SCOPE)
	set(HORDE3D_EXTENSION_LIBS ${HORDE3D_EXTENSION_LIBS} Horde3DJolt PARENT_SCOPE)
endif()
