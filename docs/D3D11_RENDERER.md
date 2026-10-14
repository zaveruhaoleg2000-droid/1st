# ARCHIVED DOCUMENT — v013 removed the D3D11 backend from compilation. It is reference material only.

# Direct3D 11 renderer (prototype v0.0.9)

This is an **actual Windows D3D11 API implementation**, not a screenshot concept and not a packaged/verified Windows build. The Windows executable must still be compiled and run on Windows to verify the device and shader path. The build and gameplay tests run in this session were Linux-only and validate the shared mesh builder / software fallback, **not actual GPU rendering**.

## Render flow

`Win32App.cpp` -> `D3D11Renderer` -> `SceneMeshBuilder` -> D3D11 vertex/index buffers -> HLSL VS/PS -> swapchain. The original `Renderer` remains functional as a software fallback and renders the global Europe map/menu as before. Press `F11` in a tactical view to toggle GPU / software. GPU init/render failure triggers automatic fallback. Existing `F6` (anti-aliasing), `F7` (wind) and `F8` (water reflections), `F10` (albedo texture size) controls are still wired. The main menu always uses its existing software UI.

## Meshes

- Terrain sampled from the existing 16-bit heightmap cache, max 160x160 cells per rebuild and **51,200 terrain triangles** or fewer; shared indexed vertices and vertex normals from local height differences.
- River has a textured water strip and cheap animated two-wave displacement and pseudo sky reflection.
- Trees are actual geometric objects: six-sided trunk (12 triangles) and two eight-sided cone crowns (16 triangles): **28 triangles per tree**, capped at 420 nearby trees. The trunk is weighted 0 and the crowns >0 for vertex shader wind. Trees use deterministic world-space cell keys to avoid shifting as the camera moves.
- Textured building boxes and simplified live soldier/squad markers.
- Static geometry uploads are cached until the camera changes sufficiently or the simulation tick changes; additional batching and spatial clip optimization are future work.

## Materials and shader

A **17-slice** `Texture2DArray` (`0` white fallback + 16 GZT1 albedo materials) with bilinear filtering and wrapped UVs. Shader implements simple directional diffuse lighting, atmospheric color blending, animated crown sway, water wave motion and approximate sky glints. The overlay is rasterized via GDI into a dynamic RGBA texture, then blended as a full-screen D3D triangle **after** optional MSAA resolve. 2x MSAA is used **only if supported** by the device; the low-end fallback is no AA. There are no shadow maps, parallax mapping, tangent-space normal mapping, real planar reflection pass, PBR roughness, or hardware instancing in this version. All textures are single-mip currently.

Texture GPU VRAM usage, excluding driver bookkeeping: 17 RGBA slices: ~1.06 MiB at 128, ~4.25 MiB at 256, ~17 MiB at 512. At 1280x720, 2x MSAA adds ~7 MiB for its color and ~7 MiB for depth; single-sample depth adds ~3.5 MiB. Scene buffer size is under 32 MiB in the test case, **not a measured total VRAM budget or framerate**.

## Building and controls

- Windows 10/11, Visual Studio 2022 C++ Desktop tools, Windows SDK, CMake.
- Run `build_windows.bat`, then `dist/PLAY.bat`. Links: `d3d11`, `dxgi`, `d3dcompiler`, `user32`, `gdi32`.
- Requires system `d3dcompiler_47.dll`, normally installed on Windows 10/11.
- D3D feature level 10.0+ with shader model 4.0.
- GPU used only in tactical Command/Direct screens; the strategic map and menu use their original software path. Preserves the older Win32 project workflow.

Note: GPU geometry currently regenerated when the simulation advances; optimization of persistent region meshes and separated dynamic actor buffers would be required before making strong FPS claims for large engagements.
