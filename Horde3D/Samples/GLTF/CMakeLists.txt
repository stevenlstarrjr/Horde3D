add_executable(GLTFViewer WIN32 MACOSX_BUNDLE app.cpp main.cpp)
target_include_directories(GLTFViewer PRIVATE
	../Framework
	../../Bindings/C++
	../../../Extensions/Overlays/Bindings/C++
	../../../Extensions/HDRI/Bindings/C++
)
if(HORDE3D_USE_GLFW)
	target_link_libraries(GLTFViewer SampleFramework Horde3D Horde3DUtils Horde3DHDRI ${GLFW_LIBRARIES})
elseif(HORDE3D_USE_SDL)
	target_link_libraries(GLTFViewer SampleFramework Horde3D Horde3DUtils Horde3DHDRI ${SDL2_LIBRARY})
endif()
add_dependencies(GLTFViewer SampleFramework)
