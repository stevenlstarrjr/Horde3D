include_directories(../Shared)

# GLTFConv is a desktop asset-build tool, just like ColladaConv.
if(NOT CMAKE_SYSTEM_NAME MATCHES "iOS|Android|Emscripten")
	include(FetchContent)
	set(FETCHCONTENT_QUIET OFF)
	FetchContent_Declare(cgltf
		GIT_REPOSITORY https://github.com/jkuhlmann/cgltf.git
		GIT_TAG v1.15
		GIT_SHALLOW TRUE
		GIT_PROGRESS TRUE
	)
	FetchContent_MakeAvailable(cgltf)
	find_package(PkgConfig REQUIRED)
	pkg_check_modules(WEBP REQUIRED IMPORTED_TARGET libwebp)

	add_library(Horde3DGLTFConverter STATIC
		converter.cpp
		converter.h
		cgltf_impl.cpp
	)
	target_include_directories(Horde3DGLTFConverter
		PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
		PRIVATE ${cgltf_SOURCE_DIR} ../Shared
	)
	target_compile_features(Horde3DGLTFConverter PUBLIC cxx_std_14)
	target_link_libraries(Horde3DGLTFConverter PRIVATE PkgConfig::WEBP)

	add_executable(GLTFConv main.cpp)
	target_link_libraries(GLTFConv PRIVATE Horde3DGLTFConverter)
endif()
