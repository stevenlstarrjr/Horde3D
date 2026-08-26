include_directories(..)
include_directories(../../Source/Horde3DEngine)
include_directories(../../Source/Horde3DUtils)
include_directories(../../Source/Shared)
include_directories(../../Bindings/C++)
include_directories(. ${CMAKE_BINARY_DIR})

add_executable(H3DUnitTests WIN32 MACOSX_BUNDLE
    test_shaderparser.cpp
    test_shader.cpp
    test_h3dutils.cpp
	test_gltf.cpp
    ../main.cpp
)

if(HORDE3D_BUILD_JOLT)
    target_sources(H3DUnitTests PRIVATE test_jolt.cpp)
    target_include_directories(H3DUnitTests PRIVATE ../../../Extensions/Jolt/Bindings/C++)
endif()

if(HORDE3D_BUILD_HDRI)
    target_sources(H3DUnitTests PRIVATE test_hdri.cpp)
    target_include_directories(H3DUnitTests PRIVATE ../../../Extensions/HDRI/Bindings/C++)
endif()

target_link_libraries(H3DUnitTests Horde3D Horde3DUtils)
target_compile_definitions(H3DUnitTests PRIVATE H3D_SOURCE_CONTENT_DIR="${CMAKE_SOURCE_DIR}/Horde3D/Binaries/Content")
