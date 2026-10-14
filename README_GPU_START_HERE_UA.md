# GPU v0.0.22 — почни з драйвера

Спочатку запусти `GPU_PROBE_WINDOWS.bat`: це перевіряє Vulkan без Visual Studio та SDK і створює `gpu_probe.txt`.

Якщо Vulkan GPU знайдено, збери гру через `build_windows.bat`, відкрий тактичну карту і надішли `graphics_backend.log`. Повідомлення `FIRST NATIVE GPU FRAME PRESENTED` означає успішне представлення native Vulkan кадру (ще не перевірене тут).

Поточний Linux тестовий контейнер має `VK_ERROR_INCOMPATIBLE_DRIVER (-9)`; перевірити native GPU 3D у ньому не вдалося.

---

# Vulkan v0.0.21 — початок роботи

Вихідний код містить окремий Vulkan 3D-pass та CPU fallback.

**Windows 11:** встановити Visual Studio 2022 (C++) + Vulkan SDK (glslangValidator), запустити `build_windows.bat` у корені архіву.

**Linux:** встановити Vulkan SDK та драйвери, виконати:
```bash
cmake -S . -B build-vulkan -DCMAKE_BUILD_TYPE=Release -DGZ_REQUIRE_VULKAN=ON
cmake --build build-vulkan -j4
ctest --test-dir build-vulkan --output-on-failure
./build-vulkan/grayzone_game_linux --renderer=vulkan
```

**Статус:** 24/24 Linux CTest пройшли без Vulkan SDK; Vulkan-прохід не був скомпільований/випробуваний з GPU. Це експериментальний GPU-код, не перевірений графічний білд.
