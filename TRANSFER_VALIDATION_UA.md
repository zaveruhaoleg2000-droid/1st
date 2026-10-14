# Контроль пакета GRAY ZONE: 1989 для Codex

Стан вихідного коду: **v0.0.22** (останній доступний вихідний архів на 14.10.2026).

## Підтверджено в Linux

- `cmake -S . -B build-linux -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DGZ_ENABLE_VULKAN=OFF -DGZ_REQUIRE_VULKAN=OFF` — успішно.
- `cmake --build build-linux -j4` — успішно.
- `ctest --test-dir build-linux --output-on-failure` — **24 / 24 успішних**, зокрема `linux_interactive_game_smoke`.
- Після перевірок скомпільовані файли з архіву **не включені**.

## Не перевірено в цьому середовищі

- Windows x64 MSVC-компіляція і запуск Windows EXE.
- Компіляція Vulkan C++ та GLSL/SPIR-V з Vulkan SDK.
- Виконання Vulkan 3D на реальному або програмному Vulkan-драйвері.
- Вимірювання 1080p/60 FPS на цільовому ПК.

## Архів містить

- `src/`, `include/`, `CMakeLists.txt`, `tests/`, `.github/`, `tools/`.
- Повну папку `assets/`, включно з геометрією й картами рельєфу, сценаріями, текстурами, аудіо та вихідними шейдерами.
- Windows скрипти для CPU / Vulkan, Vulkan GPU probe.
- `AGENTS.md`, `README_CODEX_UA.md`, `CODEX_FIRST_TASK_UA.md`, Git ignore / attributes.
- `docs/reference/tactical_checkpoint_v020_CPU.png` — **реальний CPU-кадр**, лише візуальна довідка, не підтвердження Vulkan.
