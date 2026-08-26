# HDR environment and image-based lighting extension.
option(HORDE3D_BUILD_HDRI "Build the HDRI/IBL extension into Horde3D" ON)

if(HORDE3D_BUILD_HDRI)
	add_subdirectory(Bindings)
	add_subdirectory(Source)

	set(HORDE3D_EXTENSION_INSTALLS "${HORDE3D_EXTENSION_INSTALLS}extMan().installExtension( new Horde3DHDRI::ExtHDRI() );\n" PARENT_SCOPE)
	set(HORDE3D_EXTENSION_INCLUDES "${HORDE3D_EXTENSION_INCLUDES}\n#include \"Extensions/HDRI/Source/extension.h\"\n" PARENT_SCOPE)
	set(HORDE3D_EXTENSION_LIBS ${HORDE3D_EXTENSION_LIBS} Horde3DHDRI PARENT_SCOPE)
endif()
