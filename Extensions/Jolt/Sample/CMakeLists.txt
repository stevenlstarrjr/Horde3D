include_directories(../Bindings/C++)
include_directories(../../../Horde3D/Bindings/C++)
include_directories(../../Overlays/Bindings/C++)
include_directories(${CMAKE_BINARY_DIR})
include_directories(../../../Horde3D/Samples/Framework)

if(HORDE3D_BUILD_HDRI)
	include_directories(../../HDRI/Bindings/C++)
	add_compile_definitions(H3D_JOLT_SAMPLE_HDRI)
endif()

if(${CMAKE_SYSTEM_NAME} MATCHES "Android")
	include("android.apk")
	add_library(JoltPhysicsSample SHARED app.cpp main.cpp)
	set(OUT_PATH "${PROJECT_BINARY_DIR}/Android/JoltPhysicsSample/app/src/main/jniLibs/${ANDROID_ABI}")
	set_target_properties(JoltPhysicsSample PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${OUT_PATH})
	add_custom_command(TARGET JoltPhysicsSample POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:Horde3D> ${OUT_PATH})
	add_custom_command(TARGET JoltPhysicsSample POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:Horde3DUtils> ${OUT_PATH})
	add_custom_command(TARGET JoltPhysicsSample POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy ${SDL_LIB_PATH}/libSDL2.so ${OUT_PATH})
	android_create_apk(JoltPhysicsSample "com.horde3d.joltphysics" "${PROJECT_BINARY_DIR}/Android/JoltPhysicsSample" ""
		${HORDE3D_ANDROID_PROJECT_PATH} "${PROJECT_BINARY_DIR}/Binaries/Content")
elseif(${CMAKE_SYSTEM_NAME} MATCHES "iOS")
	set(RESOURCES
		"${PROJECT_BINARY_DIR}/Binaries/Content"
		${HORDE3D_IOS_TOOLS_PATH}/LaunchScreen.storyboard
		${HORDE3D_IOS_TOOLS_PATH}/Default.png)
	add_executable(JoltPhysicsSample MACOSX_BUNDLE app.cpp main.cpp ${RESOURCES})
	set_target_properties(JoltPhysicsSample PROPERTIES
		MACOSX_BUNDLE ON
		MACOSX_BUNDLE_INFO_PLIST ${HORDE3D_IOS_TOOLS_PATH}/BundleInfo.plist.in)
	include("ios")
	SET_XCODE_PROPERTY(JoltPhysicsSample CODE_SIGN_IDENTITY ${IOS_CODE_SIGN_IDENTITY} All)
	SET_XCODE_PROPERTY(JoltPhysicsSample DEVELOPMENT_TEAM ${IOS_DEVELOPMENT_TEAM} All)
	SET_XCODE_PROPERTY(JoltPhysicsSample ENABLE_BITCODE FALSE All)
	SET_XCODE_PROPERTY(JoltPhysicsSample RESOURCE ${RESOURCES} All)
	set_source_files_properties(${RESOURCES} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
	target_link_libraries(JoltPhysicsSample "-framework AudioToolbox" "-framework AVFoundation"
		"-framework CoreAudio" "-framework CoreFoundation" "-framework CoreGraphics"
		"-framework CoreMotion" "-framework CoreVideo" "-framework Foundation"
		"-framework GameController" "-framework QuartzCore" "-framework UiKit" "-framework OpenGLES")
elseif(${CMAKE_SYSTEM_NAME} MATCHES "Emscripten")
	add_executable(JoltPhysicsSample app.cpp main.cpp)
	target_link_options(JoltPhysicsSample PRIVATE --preload-file "${SAMPLE_HORDE3D_EMSCRIPTEN_CONTENT_FOLDER}")
else()
	add_executable(JoltPhysicsSample WIN32 MACOSX_BUNDLE app.cpp main.cpp)
endif()

if(HORDE3D_USE_GLFW)
	target_link_libraries(JoltPhysicsSample SampleFramework Horde3D Horde3DUtils ${GLFW_LIBRARIES})
elseif(HORDE3D_USE_SDL)
	target_link_libraries(JoltPhysicsSample SampleFramework Horde3D Horde3DUtils ${SDL2_LIBRARY})
endif()

add_dependencies(JoltPhysicsSample SampleFramework)
