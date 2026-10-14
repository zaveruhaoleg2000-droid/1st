# GRAY ZONE: 1989 — GPU діагностика v0.0.22

**Стан:** Це виправлення діагностики для експериментального Vulkan 3D шляху, а не підтвердження роботи на GPU.

## Що перевірено 14 жовтня 2026

- Безпосередньо викликано Vulkan Loader 1.4.309 через системну бібліотеку Linux.
- `vkEnumerateInstanceVersion` повертає успіх, але `vkCreateInstance` — `VK_ERROR_INCOMPATIBLE_DRIVER (-9)`.
- У середовищі немає Vulkan SDK, шейдерного компілятора glslangValidator або драйвера Vulkan ICD. Debian package servers недоступні, тож встановити їх тут не вийшло.
- Повний проєкт із CPU fallback зібрано; **24/24 CTest пройшли**; окремий діагностичний інструмент зібрано та запущено.
- **Vulkan GPU 3D не перевірено на реальному GPU**, а сам Vulkan-код не можна скомпілювати тут без SDK.

## Як перевірити на Windows 11 без SDK

1. Розпакувати архів вихідного коду.
2. Двічі натиснути `GPU_PROBE_WINDOWS.bat` (або виконати `GPU_PROBE_WINDOWS.ps1` у PowerShell).
3. Переглянути `gpu_probe.txt`. Показники: встановлений Vulkan Loader, результат створення Instance та кількість знайдених GPU. Це не тест 3D-рендерингу.

Для **самої гри на GPU** потрібно встановити Visual Studio 2022 C++ та Vulkan SDK із glslangValidator і виконати `build_windows.bat`. Запустити `dist/PLAY.bat`, відкрити тактичну сцену й переглянути `graphics_backend.log`:

- `native 3D: READY` — native pipeline створено, але ще немає гарантії успішного GPU-кадру.
- `FIRST NATIVE GPU FRAME PRESENTED` — Vulkan-код отримав успішний результат представлення першого кадру з геометрією.
- `native 3D: UNAVAILABLE` і `Native 3D reason` — причина, чому гра залишилася на CPU-завантаженні або CPU fallback.

> Vulkan 3D зараз використовує спрощені кольори матеріалів і GPU геометрію, не всі CPU-ефекти v0.0.20. Перед релізом потрібно перевірити шейдери, depth, swapchain resize, драйвери та якість картинки на відеокарті.
