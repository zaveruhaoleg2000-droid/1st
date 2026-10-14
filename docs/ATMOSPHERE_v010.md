# v0.0.10 — Atmosphere, lighting, cast shadows and sight range

**Implemented in source code:**

- `include/gz/Atmosphere.hpp`: shared weather and time presets for CPU and GPU. Weather: Clear, Overcast, Fog, Rain, Storm. Time of day: Morning, Noon, Sunset, Night.
- `src/Renderer.cpp`: CPU software renderer directional/ambient lighting, colored sky, perspective-distance fog, terrain-following projected shadows from 3D conifers and building footprint centers (2 triangles per caster). They are dark translucent *approximations*, not physically accurate shadows or shadow maps. Weather affects tree wind, and water uses atmospheric sky tint.
- `src/SceneMesh.cpp`: a separate batch of camera-local, terrain-following shadow quads, appended after opaque triangles. Tree and building meshes remain geometrically 3D.
- `src/D3D11Renderer.cpp`: expanded HLSL constant buffer, directional light color and intensity, weather fog, sun angle; shadow batch with alpha blending and a depth-read-only state. **GPU shader/device runtime is not tested on Windows here.**
- `src/Simulation.cpp`: `setVisibilityMeters()` caps the prototype's squared-distance target acquisition in the combat solver. Previous default combat distance remains 220m. Not yet a proper terrain/obstacle raycast and not true military LOS.
- `src/Win32App.cpp`: F2 cycles daylight, F3 cycles weather, F4 toggles shadows. Environment settings are persisted in `settings.ini` (compatible with older 4-value settings). F6/F7/F8, F10, F11 remain operational. New operations and loaded saves apply current visibility limits.
- `src/weather_preview.cpp`: generates real renderer frames for six conditions, plus a low-altitude sunset frame that shows shadows.
- `tests/atmosphere_tests.cpp`: deterministic checks for sunlight/night, weather fog and wind, CPU frame differences, geometry shadow batch and combat detection ranges.

## Visibility and atmosphere presets

| Weather | Clear day engagement max | Daytime fog / atmosphere | Shadow quality |
|---|---:|---|---|
| Clear | 220 m | starts at 1,400 m | long directional geometry |
| Overcast | ~180 m | starts at 850 m | softer, dimmer |
| Fog | 75 m | starts at 55 m | very faint |
| Rain | 130 m | starts at 155 m | faint |
| Storm | 90 m | starts at 95 m | nearly absent |

At night the engagement limit is additionally reduced to 125 m. These are game-play balance parameters, not real-world visibility measurements. Daylight colors and shadow angles are artistic presets; not an astronomical simulation.

## Performance approach

- No shadow depth texture, additional full-scene render pass, volumetric raymarch or raytracing.
- Each caster adds only **two** flat shadow triangles, translucent overlay as a separate draw call in D3D11; at most 420 nearby trees and limited village props.
- CPU fog is a single post-pass over existing depth pixels; HLSL fog adds a few pixel shader operations.
- GPU scene rebuilt on camera/tick/time-of-day or shadow setting changes; weather fog/strength changes can reuse geometry.
- VRAM/RAM goals remain **targets, not proven benchmarks**. No verified Windows EXE or GPU screenshot included.

## How to test

Linux/CPU:
```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
./build/grayzone_weather_preview preview_v010
```

Windows: `build_windows.bat` using VS 2022 Desktop C++ environment. Run `dist/PLAY.bat`; F11 selects D3D11/CPU tactical renderer.

**Known limitations:** Approximate projected shadows do not deform accurately around steep objects; no shadow maps or contact-hardening; rain and storm currently use cloudiness/fog/wind rather than particle precipitation; no dynamic sun movement during a battle; no terrain obstruction or weapon-specific sight test yet. The GPU code is written but Windows execution has not been verified here.
