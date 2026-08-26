add_library(Horde3DJolt STATIC
	extension.cpp
	extension.h
	physics.cpp
	physics.h
)

target_compile_features(Horde3DJolt PRIVATE cxx_std_17)
target_include_directories(Horde3DJolt PRIVATE
	${CMAKE_BINARY_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/../Bindings/C++
	${CMAKE_SOURCE_DIR}/Horde3D/Bindings/C++
)
target_link_libraries(Horde3DJolt PUBLIC ${HORDE3D_JOLT_TARGET})
set_target_properties(Horde3DJolt PROPERTIES POSITION_INDEPENDENT_CODE ON)
