# GRAY ZONE: 1989 — вихідний код v0.0.23

Це **вихідний код**, а не Windows EXE. Початковий проєкт v0.0.22 модернізовано до v0.0.23: кешування GPU-геометрії, матеріали, освітлення й деталізовані моделі. Всередині повний `src/`, `include/`, `tests/`, `assets/` та CMake. Перевірки та межі наведено в [docs/GRAPHICS_RTX3050TI_UA.md](docs/GRAPHICS_RTX3050TI_UA.md).

Наступні етапи, пропозиції інтерфейсу, генератора рельєфу та текстур: [docs/MODERNIZATION_ROADMAP_UA.md](docs/MODERNIZATION_ROADMAP_UA.md). Це план майбутньої роботи, а не список уже реалізованих фіч.

Результати першого етапу модернізації, актуальні Debug/Release-перевірки й
Windows CPU smoke-test: [MODERNIZATION_PHASE1_UA.md](docs/MODERNIZATION_PHASE1_UA.md).

## Як почати на Windows 11

1. Розпакуй ZIP у звичайну папку, наприклад `C:\GamesDev\GRAY_ZONE_1989`.
2. Відкрий **саме цю папку** в застосунку Codex (локальне середовище Windows).
3. Встанови **Visual Studio 2022** з компонентом **Desktop development with C++** та **CMake** (зокрема CMake може входити до Visual Studio).
4. Для CPU-збірки запусти `build_windows_cpu.bat`. Для Vulkan також встанови **Vulkan SDK** (компілятор шейдерів `glslangValidator`) і запусти `build_windows.bat`.
5. Скрипти створюють `dist-win-cpu/` або `dist-win-vulkan/` з `PLAY.bat`, `GRAY_ZONE_1989.exe` та копією `assets/` **лише якщо збірка успішна**.
6. Відкрий `CODEX_FIRST_TASK_UA.md` та дай це завдання Codex.

**Порада:** перед першими змінами `git init`, `git add .`, `git commit -m "Baseline v0.0.22 before Codex"`. Файл `.gitignore` вже є. Не додавай паролі/токени до репозиторію.

## Контроль на Linux (CPU)

```sh
cmake -S . -B build-linux -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DGZ_ENABLE_VULKAN=OFF
cmake --build build-linux -j4
ctest --test-dir build-linux --output-on-failure
```

Якщо існує X11/Xvfb, додатково тестується запуск віконної гри. Запускай виконувані файли з кореня проєкту, де розташована папка `assets`.

## Поточний стан

- Є меню, стратегічна карта Європи, сценарії, тактична симуляція й контрактний цикл.
- Є CPU-рендерер, графічний каркас Vulkan, експериментальна native GPU сцена і заглушка DirectX.
- Windows x64 Vulkan/CPU EXE зібрано; CPU запуск із готової папки перевірено під Wine. Native Vulkan кадри перевірено через Mesa Lavapipe та synchronization validation; фізична відеокарта й Windows 11 ще не перевірені. Результати оптимізації: [docs/GPU_OPTIMIZATION_UA.md](docs/GPU_OPTIMIZATION_UA.md).
- `GPU_PROBE_WINDOWS.bat` допоможе перевірити Vulkan-драйвер у Windows. Для повного звіту дивись `GPU_DIAGNOSTICS_v022_UA.md`.
- Графічні цілі: 4 потоки CPU / 4 ГБ RAM / 2 ГБ VRAM / до 1 ГБ встановлення; 1080p 60 FPS — лише орієнтир.

## Ключові місця в коді

- Vulkan: `src/VulkanBackend.cpp`, `src/VulkanScenePass.cpp`, `assets/shaders/vulkan/native_scene.vert`, `native_scene.frag`.
- Підготовка геометрії: `src/SceneMesh.cpp`, `src/GeometryStaging.cpp`, `src/RenderBackend.cpp`.
- Windows: `src/Win32App.cpp`.
- Логіка гри: `src/Simulation.cpp`, `src/Contract.cpp`, `src/Campaign.cpp`.
- Автоматичні перевірки: `tests/`, `ctest`.

Окремі попередні версії гри зберігати в цій папці не потрібно: Codex має працювати з **одним** актуальним проєктом та Git-історією.
