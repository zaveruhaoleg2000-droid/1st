# GRAY ZONE: 1989 — Vulkan-first architecture (v0.0.13)

## What actually runs now

- `gzcore` — simulation, terrain, political map, sound, campaign, UI; no Vulkan or D3D headers.
- `gzrender` — `RenderBackendHost`, backends, independent window abstraction, fallback policy.
- `VulkanBackend` — Vulkan instance, native Xlib/Win32 surface, graphics/present queue selection, FIFO swapchain, host-visible staging buffer, copy into swapchain images, per-frame fence/semaphores, resize and device teardown.
- `DirectXCompatStub` — explicit compatibility slot. Old D3D11 implementation archived in `src/legacy/`, excluded from build. **Not functional D3D rendering.**
- `SoftwareBridge` — caller-owned Win32 GDI or X11 framebuffer presentation after GPU failure; functional fallback.

**Important limitation:** this milestone uses Vulkan for window-system presentation and copies CPU-rasterized pixels to the Vulkan swapchain. Terrain, trees, water and units are **not yet rasterized in Vulkan**. The GLSL sources in `assets/shaders/vulkan/` are **source templates** for native GPU geometry and are not yet compiled, attached to descriptor sets or executed. Therefore **do not claim a Vulkan FPS speedup or Vulkan-native 3D** from this version.

## Frame / data flow

```
Simulation, Campaign, UI (gzcore)
          |
        Scene state
          |
 CPU Renderer (existing)  <--- next milestone replaces this with native GPU draw batches
          |
 FramePixels BGRA8           + optional GeometryView (mesh stream protocol prepared)
          |
 RenderBackendHost::submit(RenderSubmission)
          |
     VulkanBackend (preferred) ---> VkSwapchainKHR / copy buffer / queue submit / present
          | failure
     SoftwareBridge            ---> OS GDI StretchDIBits / X11 XPutImage
```

One renderer is selected at a time. There is no parallel duplicated GPU work.

## Execution graph and residency

`RenderGraphPlan` validates stage ordering and dependencies. The **active** GPU graph contains two passes (upload + present); a separate validated eleven-pass target graph specifies the order for terrain, geometry, units, vegetation, water, lighting, weather and UI. The latter is **not executed yet**. `GpuBudgetLedger` reserves/reclaims resource bytes before future mesh/texture residency operations and has independent regression tests. The Vulkan staging buffer also enforces `GraphicsLimits::streamingBudgetBytes` at allocation.

## Growth points

1. `GeometryView` / `RenderSubmission`: immutable buffer views with byte counts, index lists and tick/time fields; integrate `SceneMeshBuilder` when native GPU rasterization becomes available.
2. Create render graph passes: depth pre-pass, opaque terrain, vegetation instancing, water transparency, silhouettes/units, UI, weather overlay.
3. Allocate persistent device-local vertex/index buffers; stage only dirty chunks and track transfer fences. Integrate render streaming budgets (128 MiB staging default, 2 GiB VRAM target).
4. Compile `assets/shaders/vulkan/*.vert|frag` into SPIR-V with glslc or glslangValidator. Add `VkPipelineLayout`, descriptor sets, texture arrays, `VkGraphicsPipeline` and uniform ring buffers.
5. Support 2 frames-in-flight, descriptor/pipeline caching, per-frame transient allocations, GPU timestamps; add resize/recreation and validation layers in debug mode.
6. Implement Vulkan-native global-map triangulation, political overlay and terrain LOD so the game is actually GPU-rendered.
7. Cross-compare Vulkan with previous D3D11 backend before reconstructing the DirectX compatibility slot. Legacy code is reference only.

## Build / run

Windows: Visual Studio 2022 with C++ Desktop, CMake and LunarG Vulkan SDK. Run `build_windows.bat`; it requires Vulkan SDK (`GZ_REQUIRE_VULKAN=ON`) and packages `dist/`. Runtime also needs a working Vulkan-capable driver. `graphics_backend.log` reports choice and fallback.

Linux: install `libvulkan-dev`, `libx11-dev`, Vulkan ICD GPU driver, `cmake`, C++20 compiler. `cmake -S . -B build -G Ninja -DGZ_REQUIRE_VULKAN=ON && cmake --build build -j4 && ctest --test-dir build --output-on-failure`.

If Vulkan SDK is absent use `-DGZ_REQUIRE_VULKAN=OFF`; the project compiles an explicit unavailable-backend stub and retains the software renderer. This is **not Vulkan GPU support**. Use `--renderer=auto|vulkan|software|directx` on Linux; `F11` toggles Vulkan/software during gameplay. Windows stores a graphics preference in the last field of `settings.ini`; `F11` toggles Vulkan/software.

## Test coverage / not yet tested

- Tested on development Linux: entire application can compile without Vulkan SDK, backend selection/resize/fallback tests, campaign and graphic regressions, X11 launch under Xvfb.
- Not tested in this environment: Vulkan native instance/swapchain creation against a GPU; Windows executable and DirectX placeholder; SPIR-V compilation and native Vulkan mesh pipeline.
- Vulkan SDK-dependent source is guarded in CMake and requires additional testing on actual hardware. GPU performance cannot be inferred from existing CPU benchmark results.
