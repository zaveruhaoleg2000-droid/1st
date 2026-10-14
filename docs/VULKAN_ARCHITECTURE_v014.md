# Vulkan engine growth base — v0.0.14

## New subsystems

- `EuropeBackdrop::selectAt`: operates on already-displaced screen-space vertices of the exact heightmap mesh; one-time O(N) scan on click; 39,321 vertices, negligible compared with a frame.
- `Renderer::selectGlobalAt`: bridges map point selection to Windows X11 frontend; `EuropeBackdrop::render` produces marker and geoposition panel; zoom/pan preserved.
- `FrameMeter`: 60-frame O(1) rolling-window averages for scene and rendering/present cost, with safeguards for invalid samples.
- `DebugHud`: uses game PGM glyph atlas to write directly to CPU framebuffer so Vulkan transfer and X11/GDI fallback display identical pixels. F1 on both platforms.
- `GeometryStagingCache`: owns mesh copies, validates vertex stride and index bounds, enforces 128 MiB default budget, returns immutable revision-keyed geometry and avoids redundant copy if unchanged. No Vulkan allocations are performed; GPU residency is next.

## GPU progression

1. Own the `GeometryStagingCache` from the scene/resource scheduler. Name meshes by stable tile/object IDs.
2. Implement device-local vertex and index buffer handles for revisions, staging transfer jobs, fences, and residency eviction.
3. Compile SPIR-V with glslangValidator / glslc; create Vulkan pipeline layouts, depth buffer and material descriptors.
4. Record indexed draws, then move the Europe and tactical terrain mesh to Vulkan. Preserve DirectX as a future independent backend.
5. Profile CPU-vs-GPU workload, check validation layers and hardware before claiming speed gains.

Current Vulkan backend still uploads pre-rendered CPU pixels to a swapchain. The cache in step 1 exists as a tested module but is not yet connected to Vulkan transfer commands.
