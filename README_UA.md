# Vulkan GPU diagnostic update — v0.0.22

**Що дійсно перевірено:** Vulkan Loader 1.4.309 доступний, але `vkCreateInstance` дає `VK_ERROR_INCOMPATIBLE_DRIVER (-9)` у поточному тестовому контейнері. GPU 3D не запускався, Vulkan SDK тут немає. 24/24 CTest CPU-гри пройдено.

- `GPU_PROBE_WINDOWS.bat` запускає системну Vulkan-діагностику без встановлення SDK.
- `gpu_probe.txt` показує результат Vulkan Loader/Instance/GPU discovery.
- `graphics_backend.log` у грі тепер повідомляє native 3D READY/UNAVAILABLE й успішне представлення першого GPU-кадру.
- Для Vulkan-збірки гри потрібні Visual Studio 2022 C++, Vulkan SDK і `glslangValidator`.

Деталі — у `GPU_DIAGNOSTICS_v022_UA.md`.

---

# GRAY ZONE: 1989 — Graphics v0.0.20 / бойовий прототип

Це **повний вихідний код** прототипу з бойовим вузлом: блокпост, мішки з піском,
вирви, колії, дим, уламки, пошкоджена техніка, режим фото `O` та вкл./викл.
декорацій `Z`. Контракт, тактичний режим, політична карта, збереження та звук
також входять у проєкт. Повноцінної GPU-растеризації через Vulkan ще немає.

## Windows — створити виконуваний .exe

1. Установіть **Visual Studio 2022** з компонентом **Desktop development with C++**.
2. Установіть CMake (якщо не доступний у терміналі).
3. Розпакуйте цей ZIP в папку з правом запису.
4. Запустіть `build_windows_cpu.bat`. Vulkan SDK **не потрібен**.
5. У разі успіху гра буде в `dist-cpu/PLAY.bat`.

**Увага:** Windows-компіляцію та запуск тут не перевірено. Скрипт — вихідний код
для самостійної Windows-збірки, а не гарантовано готовий .exe.

## Linux

`cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON`

`cmake --build build -j4`

`ctest --test-dir build --output-on-failure`

`./build/grayzone_game_linux --renderer=software`

Запускати зі **кореня проєкту**, де лежить `assets/`.
Linux x64 з X11 або XWayland і libX11.

## Грати

Нова гра → «Варшавський коридор» → `J/K` чисельність → `Enter` розгорнути загін.
`1` тактика, `2` боєць, `3` стратегічна карта, `ПКМ` наказ, `Tab` наступний загін,
`T` реальний/покроковий режим, `Enter` хід, `F5/F9` зберегти/завантажити,
`O` фоторежим, `Z` увімк./вимк. бойовий декор, `F1` діагностика, `F12` якість.

## Статус

Linux Release build — пройдено 23/23 CTest та бойовий smoke-test у чистій
папці (2026-10-14). Декорації не мають фізики/справжніх укриттів; GPU Vulkan
скелет є, але бойові кадри малюються CPU. На Windows реальна збірка не перевірена.
