# Horde3D HDRI and image-based lighting extension

This extension turns a floating-point equirectangular environment into the resources needed for
split-sum PBR image-based lighting:

- an HDR environment cubemap;
- a cosine-convolved diffuse irradiance cubemap;
- five GGX-prefiltered specular cubemaps covering roughness 0 through 1;
- a two-channel integrated BRDF lookup texture.

It is enabled by default and can be disabled with `-DHORDE3D_BUILD_HDRI=OFF`.

## Usage

Add a Radiance `.hdr` texture before loading resources, then generate the IBL set after it is
loaded:

```cpp
#include <Horde3D.h>
#include <Horde3DHDRI.h>

H3DRes source = h3dAddResource(H3DResTypes::Texture, "hdri/environment.hdr",
    H3DResFlags::NoTexCompression | H3DResFlags::NoTexMipmaps);

// Load normal Horde3D resources here.

H3DHdriSettings settings;
settings.environmentSize = 256;
settings.irradianceSize = 32;
settings.specularSize = 128;
settings.brdfLutSize = 256;
settings.sampleCount = 256;

H3DHdriIbl ibl;
if (!h3dHdriCreateIbl(source, "OutdoorIBL", &settings, &ibl)) {
    // Source must be a loaded 2D RGBA32F texture.
}
```

The included `shaders/pbr_ibl.shader` uses the following sampler names. A material may initially
point them at placeholder textures; `h3dHdriBindIbl(material, &ibl)` replaces all seven bindings:

- `iblIrradiance`
- `iblSpecular0` through `iblSpecular4`
- `iblBrdfLut`

The shader's `baseColor` uniform is linear RGBA. `pbrParams` contains metallic, roughness, ambient
occlusion, and IBL intensity. Use Horde3D's HDR pipeline for values above display range.

Call `h3dHdriReleaseIbl(&ibl)` before releasing Horde3D. The generated levels are separate
cubemaps because Horde3D's automatic mip generation would otherwise overwrite GGX-filtered mip
data.
