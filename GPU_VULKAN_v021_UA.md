# GRAY ZONE: 1989 — Vulkan GPU 3D, v0.0.21 (14.10.2026)

## Реалізовано у вихідному коді

- **Native scene path:** `SceneMeshBuilder` готує сітку рельєфу, води, селища, дерев та маркерів. Програма передає `SceneVertex` і 32-бітні індекси через RHI (`RenderSubmission.worldMesh`).
- **Відеокарта растеризує трикутники:** новий `VulkanScenePass` містить graphics pipeline, vertex/index buffers, `vkCmdDrawIndexed`, color attachment, depth image/тест глибини, framebuffer на зображення swapchain, push constants із VP-матрицею/освітленням/туманом. Матеріали поки задаються простою палітрою у фрагментному GLSL-шейдері, без повного texture atlas.
- **Обхід CPU-растеризації** в тактичному режимі, якщо `nativeSceneReady()`. Меню і глобальна карта продовжують малюватися CPU. Для відмови GPU платформи викликають повторну CPU-відмальовку.
- **Шейдери** `assets/shaders/vulkan/native_scene.vert`, `.frag`; при знайденому `glslangValidator` CMake автоматично формує `.spv`. Тільки за наявності `.spv` Vulkan native pass запускається.
- **Телеметрія:** `nativeSceneFrames`, `nativeSceneTriangles`, `meshUploadedBytes` окремо від звичайних кадрів, переданих GPU для показу.
- **Сумісність:** CPU та заглушка DirectX залишаються; Vulkan 1.0 API, без обов’язкових розширень на кшталт descriptor indexing.

## Статус перевірки

**24/24 Linux CTest пройдено** разом з перевіркою матриці камери, формату вершин та контрольним запуском Linux-застосунку. Це перевірка збірки гри і CPU-резерву.

**GPU-path у цьому середовищі НЕ скомпільовано та НЕ запущено:** тут немає Vulkan SDK headers/glslangValidator/драйвера. Тому це код нового GPU-шляху, не підтверджений GPU-білд. Нема виміряних FPS на GPU, SPIR-V у пакеті вихідного коду генерується на твоїй машині.

## Windows 11 — підготувати тестовий GPU-білд

1. Встановити Visual Studio 2022 з `Desktop development with C++` та [Vulkan SDK](https://vulkan.lunarg.com/sdk/home). У SDK має бути `glslangValidator.exe` і працювати `VULKAN_SDK`.
2. Розпакувати весь source ZIP, запустити `build_windows.bat`. Скрипт вимагає Vulkan і перевіряє наявність згенерованого SPIR-V, складає `dist/GRAY_ZONE_1989.exe` та ресурси.
3. Запустити `dist/PLAY.bat`. `F11` — перемикання між Vulkan і CPU; у бойовому тактичному режимі Vulkan рендерить mesh GPU, а глобальна карта CPU.
4. Для CPU-only Windows запуску можна використати `build_windows_cpu.bat` — Vulkan SDK не потрібен.

## Linux — підготувати GPU-білд

`sudo apt install cmake g++ libvulkan-dev glslang-tools libx11-dev mesa-vulkan-drivers` (потрібні реальні драйвери).

```
cmake -S . -B build-vulkan -DCMAKE_BUILD_TYPE=Release -DGZ_REQUIRE_VULKAN=ON
cmake --build build-vulkan -j4
ctest --test-dir build-vulkan --output-on-failure
./build-vulkan/grayzone_game_linux --renderer=vulkan
```

CMake генерує `assets/shaders/vulkan/native_scene.{vert,frag}.spv` у дереві проєкту; запускати з кореня, де лежить папка assets.

## Чесні обмеження першої GPU-версії

- GPU матеріали зараз кольорові, поки немає texture atlas, прозорої води, повних спецефектів, графічного HUD поверх Vulkan-сцени й сучасних тіней.
- `SceneMeshBuilder` ще використовує спрощені шаблони об’єктів, тому графіка GPU може виглядати простіше за CPU-картинку v0.0.20: нові барикади та деякі декорації залишилися тільки в CPU renderer.
- Буфери мають HOST_VISIBLE memory і оновлюються кожного тактичного кадру: це справжня GPU-растеризація, але ще не оптимальне завантаження геометрії в DEVICE_LOCAL і не кешування mesh по чанках.
- Вбудованої підтримки Direct3D наразі немає, лише ABI-заглушка. Ціль 2 GiB VRAM ще не підтверджена реальним тестуванням.
- Поточна збірка Linux з цього середовища містить **software fallback**, не Vulkan. Не плутати з підтвердженим GPU-рендерингом.

## Наступна перевірка на ПК з GPU

Потрібно протестувати старт/resize, ввімкнути валідаційні шари Vulkan, перевірити кадр/зображення, асинхронне завантаження mesh, потім перенести GPU-матеріали і техніку. Надішли журнал запуску або скриншот і перевіримо баги.
