# Історія змін

## v0.0.4 — легкі візуальні ефекти

- Edge-adaptive anti-aliasing з перемикачем F6 і малим додатковим буфером.
- Детермінована анімація крон дерев від вітру, F7.
- Поверхня річки з рухомими хвилями, приблизними відображеннями неба й прибережної рослинності, F8.
- Візуальні ефекти відокремлені від бойової симуляції.
- 4/4 автоматичні тести включно з новою перевіркою графічних ефектів.

## v0.0.3 — покроковий цикл та виправлення ШІ

- Збереження наказів при передаванні керування ШІ, покрокові фази, просторова оптимізація розрахунків.

## v0.0.2 — перший 3D-прототип

- Програмний растеризатор, рельєф, дерева, підрозділи, три режими, Win32 UI.

## v0.0.1 — симуляційне ядро

- Військовослужбовці, відділення, три режими, збереження, пул робітників.

## v0.0.5 Pixel-to-Vertex Terrain — 2026-10-13
- Added actual 1025x1025 signed 16-bit pixel heightmaps for each 3x3km square.
- Added edited PGM source and binary GZT exchange format, checksum, import/export tool.
- Added 16x16 chunk LOD renderer with skirts and on-demand LRU world cache.
- Added image-authored hill in sector 1,0; 16-bit source terrain and actual game screenshots.
- Linux compile and five automated suites confirmed; Direct3D and Windows runtime still unverified.

## v0.0.11 (2026-10-13) — weather particles and local light prototypes
- Added shared deterministic point/spot emitters: campfire flicker, paired vehicle headlamps, timed explosion flash (manually triggered with G).
- Added rain, heavy storm rain and drifting snow to software framebuffer and Windows D3D11 HUD overlay.
- Added wet road/terrain shading and immediate snow cover overlay in CPU software and GPU material shaders.
- Weather enum includes Snow, with adjusted visibility and wind.
- New controls L/P/G and persistence in settings.ini.
- Dedicated automated checks for light attenuation, precipitation determinism, flash lifetime, and frame pixel changes.
- GPU code and HUD pathway are not yet executed on real Windows hardware.
