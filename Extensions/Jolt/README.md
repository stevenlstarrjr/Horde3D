# Horde3D Jolt Physics extension

This optional extension binds Jolt Physics rigid bodies to Horde3D scene nodes. It currently
supports box, sphere, and capsule shapes with static, kinematic, and dynamic motion.

## Build

Enable the extension when configuring Horde3D:

```sh
cmake -S . -B build -DHORDE3D_BUILD_JOLT=ON
cmake --build build
```

CMake first looks for an installed `Jolt` package. If none is found, it fetches the pinned release
in `HORDE3D_JOLT_VERSION`. Set `HORDE3D_JOLT_FETCH=OFF` to require a system package instead.

The extension requires C++17 when enabled. The rest of Horde3D retains its existing language level.

## Sample

Build the interactive sample together with the extension:

```sh
cmake -S . -B build -DHORDE3D_BUILD_JOLT=ON -DHORDE3D_BUILD_EXAMPLES=ON
cmake --build build --target JoltPhysicsSample
```

Run `JoltPhysicsSample` from the generated `Binaries` directory so it can find the copied
`Content` directory. The scene contains a static floor, a moving kinematic platform, and falling
dynamic boxes and spheres. A body is launched automatically every few seconds; press `F` to launch
it manually or `R` to reset the scene. When the HDRI extension is enabled, the sample also converts
`hdri/ferndale_studio_12_4k(1).hdr` into image-based lighting and renders its boxes with the
included PBR shader.
Press `I` to disable or enable only the IBL contribution while leaving the HDRI sky visible; this
makes the environment lighting effect directly comparable.

## Minimal use

```cpp
#include <Horde3D.h>
#include <Horde3DJolt.h>

H3DNode floorNode = h3dAddGroupNode(H3DRootNode, "floor");
h3dSetNodeTransform(floorNode, 0, -0.5f, 0, 0, 0, 0, 1, 1, 1);
h3dJoltAddBoxBody(floorNode, 20, 0.5f, 20, H3DJoltMotionType::Static, 0);

H3DNode ballNode = h3dAddGroupNode(H3DRootNode, "ball");
h3dSetNodeTransform(ballNode, 0, 5, 0, 0, 0, 0, 1, 1, 1);
H3DJoltBody ball = h3dJoltAddSphereBody(ballNode, 0.5f,
    H3DJoltMotionType::Dynamic, 1.0f);

// Once per frame. A fixed 60 Hz step is recommended.
h3dJoltStep(1.0f / 60.0f, 1);
```

Dynamic bodies write their simulated world transforms back to the bound Horde nodes. Kinematic
bodies read their target transforms from Horde before each step. Collision dimensions are explicit
world-space values and are not inferred from render-node scaling.

A body is automatically removed if its bound Horde node no longer exists. Applications may also
remove it explicitly with `h3dJoltRemoveBody`.
