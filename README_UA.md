# GRAY ZONE: 1989 — графіка v0.0.23

Native Vulkan малює текстуровану 3D-сцену з об'ємними бійцями, танком, БТР, вантажівкою, будинками, деревами, тінями, MSAA та анімованою водою. Підготовка сцени кешується; геометрія передається частинами.

Перевірено Linux CPU Debug/Release 25/25, Vulkan 26/26 із Mesa Lavapipe та synchronization validation; Windows x64 CPU/Vulkan 23/23 під Wine. Windows native smoke подав шість текстурованих Vulkan кадрів без CPU fallback. Фізична RTX 3050 Ti та Windows 11 ще не тестувалися. Докладно: [графіка й перевірки](docs/GRAPHICS_RTX3050TI_UA.md).

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
також входять у проєкт. GPU та CPU рендерери мають різні набори деталей; декоративні об'єкти не додають фізику або AI техніки.

## Windows — створити виконуваний .exe

1. Установіть **Visual Studio 2022** з компонентом **Desktop development with C++**.
2. Установіть CMake (якщо не доступний у терміналі).
3. Розпакуйте цей ZIP в папку з правом запису.
4. Запустіть `build_windows_cpu.bat`. Vulkan SDK **не потрібен**.
5. У разі успіху гра буде в `dist-cpu/PLAY.bat`.

У готовому пакеті запустіть `PLAY.bat`; для RTX 3050 Ti 4 ГБ доступний `PLAY_HIGH.bat`, а також середня, низька якість та CPU-запуск. Для native Vulkan-збірки з вихідного коду потрібний Vulkan SDK.

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

Native Vulkan кадри та ресурси перевірені програмним Vulkan-драйвером; Windows EXE зібрані й перевірені під Wine. Декорації не мають фізики/справжніх укриттів. FPS на фізичній відеокарті та Windows 11 потребують перевірки на цільовому ПК.
