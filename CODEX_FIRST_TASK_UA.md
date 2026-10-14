# Перше завдання Codex — стабільний Windows 11 GPU-білд

Працюй із відкритим локальним C++-проєктом **GRAY ZONE: 1989**, v0.0.22. Перед змінами прочитай `AGENTS.md`, `README_CODEX_UA.md`, `README_GPU_START_HERE_UA.md`, `CMakeLists.txt` і файли Vulkan.

**Мета:** створити й реально перевірити Windows 11 x64 версію гри та перший **нативний Vulkan 3D-кадр**. Не використовуй концепт-арт як доказ роботи GPU.

## Послідовність

1. Перевір CMake-конфігурацію, структуру `assets/`, C++20, доступність Visual Studio та Vulkan SDK.
2. Збери **CPU fallback** через `build_windows_cpu.bat`, виправ помилки; запусти `ctest -C Release` та гру зі зібраної `dist-win-cpu`.
3. Запусти `GPU_PROBE_WINDOWS.bat`, визнач Vulkan-драйвер і його можливості; зафіксуй `gpu_probe.txt`.
4. Збери Vulkan шлях через `build_windows.bat`. Перевір `glslangValidator`, SPIR-V, Vulkan instance/device/swapchain, vertex/index buffers, depth attachment, камеру, pipeline, синхронізацію і presentation.
5. Запусти гру у Vulkan режимі й перевір журнал `graphics_backend.log`. Якщо GPU-кадр не представлений — діагностуй, виправ і повтори. **Не стверджуй, що Vulkan працює, поки це не підтверджено запуском.**
6. Збережи fallback CPU і DirectX stub. Не ламай бої, кампанію чи збереження.
7. Підготуй готову папку Windows x64 (`PLAY.bat`, `.exe`, `assets/`), реальні скриншоти й короткий звіт: компілятор, відеокарта/драйвер, кількість пройдених тестів, фактична продуктивність, відомі обмеження.

Якщо драйвер або SDK відсутні, зупинися на чесному діагностичному статусі з конкретними інструкціями; не підмінюй GPU-результат CPU-знімками.

**Не починай із нових моделей або візуальних ефектів. Спершу Windows-сумісність та Vulkan pipeline.**
