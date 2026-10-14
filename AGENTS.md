# GRAY ZONE: 1989 — instructions for Codex

## Project identity
- Ukrainian-language alternate-history military strategy / tactical prototype, set around 1989 in Eastern Europe.
- Main implementation: **C++20 + CMake**. There is no Unreal/Unity engine here.
- Strategic Europe map + procedural 3x3 km tactical environments + PMC contract/battle loop.
- Project snapshot: **v0.0.22** (2026-10-14), taken from the latest supplied Vulkan/diagnostics source archive.

## Communication
- Work with the user **in Ukrainian**. Preserve Ukrainian UI text and existing lore/scenarios.
- Work in source files, compile, test, and report exact commands and failures.
- Never call a conceptual render or a CPU screenshot a verified native Vulkan output.

## Priority / acceptance criteria
1. **Build a real Windows 11 x64 EXE**, smoke-test on the user's Windows machine, retain assets in distribution.
2. Implement/repair **native Vulkan GPU rasterization**: real vertex/index draws, compiled SPIR-V, depth, swapchain presentation; verify with an actual Vulkan driver and `graphics_backend.log`.
3. Preserve **automatic CPU fallback** and retain DirectX compatibility stub (do NOT pretend the DirectX backend works).
4. Keep campaign, battle, saves, scenario files, and graphics regression tests working.
5. Profile objectively before optimizing; produce real in-engine screenshots and measured FPS.

## Architecture and entry points
- `CMakeLists.txt` — targets, SDK detection, CTest.
- `src/Win32App.cpp` / `src/X11App.cpp` — windowing + frame loop.
- `src/VulkanBackend.cpp`, `src/VulkanScenePass.cpp` — Vulkan backend and experimental native 3D.
- `src/RenderBackend.cpp`, `src/GeometryStaging.cpp`, `src/SceneMesh.cpp`, `src/RenderGraph.cpp` — graphics abstraction/geometry.
- `src/Renderer.cpp` — CPU reference renderer and tactical visuals.
- `src/Simulation.cpp`, `src/Contract.cpp`, `src/Campaign.cpp` — gameplay/saves.
- `assets/shaders/vulkan/native_scene.vert|frag` — Vulkan source shaders; compile `.spv` using `glslangValidator`.
- `assets/` — runtime assets, **ship all subdirectories**.
- `tests/` — regressions.
- `src/DirectXCompatStub.cpp` — placeholder; leave it optional.

## Build commands
**Windows CPU baseline** (VS2022 Desktop development with C++, CMake):
`build_windows_cpu.bat`

**Windows Vulkan** (also install Vulkan SDK + `glslangValidator`):
`build_windows.bat`

**Linux CPU baseline**:
```sh
cmake -S . -B build-linux -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DGZ_ENABLE_VULKAN=OFF
cmake --build build-linux -j4
ctest --test-dir build-linux --output-on-failure
```
Run from project root so relative `assets/` paths resolve.

## Honest known status (as of source snapshot)
- Linux CPU baseline previously reported 24/24 tests passing. The new transfer package is also retested separately where supported.
- **Windows native EXE was not built or smoke-tested in the original container**.
- **Native GPU Vulkan draw remains unverified on a GPU**. Runtime loader alone is not proof.
- CPU fallback is necessary for reliability and for testing with no Vulkan SDK/GPU.
- Tactical visual props are decorative; do not assume they provide physics, collision, cover or fully functional vehicle AI.

## Resource constraints / technical goals
- Minimum-oriented target: **4 CPU threads, 4 GiB system RAM, 2 GiB VRAM**.
- Game install target: **≤1 GiB**.
- Aspirational performance: ~60 FPS at 1080p on supported hardware; **not yet measured/guaranteed**.
- Prefer bounded resident memory, caching, LOD, instancing, chunk visibility, background streaming.

## Coding & validation discipline
- Work in small coherent steps; test `ctest` after each change. Build both CPU and Vulkan configurations on Windows when available.
- Do not discard the CPU renderer while Vulkan path is unfinished.
- Avoid unrelated broad refactors, platform-specific assumptions and checked-in build binaries.
- Preserve older tests and save compatibility; add tests for new GPU math/data structures.
- Keep shader sources in the repository; compile SPIR-V in the build, not as mysterious binary blobs.
- Use Git commits for working milestones; keep a changelog and record untested paths clearly.
- Before claiming a playable Windows release, run the built `GRAY_ZONE_1989.exe` **from the packaged distribution directory** and verify required assets and logs.

## First work order
Read `CODEX_FIRST_TASK_UA.md` and `README_CODEX_UA.md`. Diagnose and complete the Windows GPU path rather than adding decorative content before the executable is reliable.
