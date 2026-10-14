# Environment and Local Lights — v0.0.11

## Visual implementation

**Sources**: 4 fixed light records, no extra shadowmaps: one orange campfire, two forward-facing white headlamps, and one fading orange explosion flash. Shared `EnvironmentFX.hpp` computes deterministic parameters from visual-time and a manual `ExplosionEvent`. Light locations are *demo scene anchors*, not attached to simulated moving entities yet. The user can trigger a visual explosion via **G** at a point ahead of the current camera focus; it does not create physical blast damage.

**Attenuation**: quadratic `max(0, 1 - d² / r²)²` plus cone attenuation for headlamps. CPU only evaluates light where triangles overlap a source's horizontal footprint; GPU HLSL evaluates four sources per fragment. The CPU mode uses post-draw small gradient sprites for the luminous fire/explosion core; D3D11 does not currently render equivalent sprites, only shader illumination.

**Weather**: Clear, Overcast, Fog, Rain, Storm, **Snow**. Weather affects fog, wind, visibility, material wetness/snow cover. Rain/storm/snow particles are deterministic screen-space segments/dots. CPU draws them into its framebuffer; GPU renders them in the existing GDI/HUD texture overlay (no new draw call). Particles have no collision, splashes, puddle simulation, or persistent accumulation. Snow cover appears immediately as a material blend, not physical snow buildup.

**GPU**: new FrameConstants cbuffer fields: 1x float4 surface and 3x4 float4 light arrays; Windows build/runtime not verified.

## Controls

- `F2`: time of day.
- `F3`: cycle weather, including Snow.
- `F4`: toggle simplified geometry-projected shadows.
- `L`: toggle local lights (fire/headlights/flash).
- `P`: toggle screen-space precipitation.
- `G`: trigger short visual explosion ahead of camera; debug/test only.
- `F11`: D3D11 / software fallback toggle.

## Tests

Run `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j4 && ctest --test-dir build --output-on-failure`. The automated `environment_lights_precipitation` regression test compares deterministic framebuffers and verifies attenuation/visibility; Windows D3D11 should be built and tested on a Windows machine.

## Known limitations

- 3D models and light anchors are deliberately simple test assets.
- No authored particle textures or decals; precipitation is screen-space.
- No damage simulation from visual `G` explosion.
- Fire/headlights use static village positions, not entity components yet.
- No GPU runtime profiling or real-Windows test; the target of 4 CPU threads, 4 GiB RAM, 2 GiB VRAM, 1 GiB disk is not measured yet.
- Software renderer remains a correctness fallback and may have low frame rate at large resolutions.
