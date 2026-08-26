add_library(Horde3DHDRI STATIC
	extension.cpp
	extension.h
	ibl.cpp
	ibl.h
)

target_compile_features(Horde3DHDRI PRIVATE cxx_std_14)
target_include_directories(Horde3DHDRI PRIVATE
	${CMAKE_BINARY_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/../Bindings/C++
	${CMAKE_SOURCE_DIR}/Horde3D/Bindings/C++
)
set_target_properties(Horde3DHDRI PROPERTIES POSITION_INDEPENDENT_CODE ON)
