# Перший етап модернізації — 14.10.2026

Зміни забезпечують достовірні тести та перевірку Windows CPU-дистрибутива.
Ігрові механіки й формат збережень не змінено.

## Що змінено

- У шести тестових файлах `assert` замінено на спільний `GZ_CHECK`, який
  виконує умову один раз і повертає помилку процесу незалежно від `NDEBUG`.
  Нові перевірки слід писати через цей механізм або інші завжди активні check/require.
- GPU-камера перевіряється окремо для подвоєння роздільності зі збереженням
  співвідношення сторін і для зміни співвідношення сторін.
- Photo-mode тест використовує ту саму камеру біля живого бійця для кадрів
  із бійцями та без них. Перевірки декорацій і диму збережені.
- Windows RECT координати мають явний тип у `std::max`; WinMM є залежністю
  `gzcore`, тому доступний також тестам і утилітам.
- MSVC використовує статичний CRT за замовчуванням, а MinGW — Unicode entry point.
  GCC Debug використовує стандартні Debug-параметри без примусового `-O2`.
- Windows `--smoke-test` вимагає assets поряд із EXE, примусово вибирає CPU,
  завантажує меню/сценарії/текстури, запускає контракт і виконує бій до результату.
  Пише `smoke_test.log` і три CPU PPM-кадри; повертає 0 лише після успіху.
  Користувацькі слоти збережень не використовуються. Це не перевірка звукового
  пристрою або Vulkan, і цей режим не вимірює фактичний FPS Windows-гри.
- `verify_windows_package.ps1` звіряє всі assets за SHA256, запускає EXE з
  лімітом 60 секунд і перевіряє актуальний журнал та кадри. CPU batch-скрипт
  виконує цю перевірку після пакування.
- GitHub Actions має Linux/Windows × Debug/Release, забороняє нульовий
  CTest-прогін та зберігає результати й Windows-дистрибутиви як артефакти.

## Фактично перевірено тут

| Перевірка | Результат |
|---|---|
| Linux GCC 14.2 Debug CPU | Збірка успішна, CTest 24/24 |
| Linux GCC 14.2 Release CPU | Збірка успішна, CTest 24/24 |
| GZ_CHECK із `-O3 -DNDEBUG` | Умова виконується один раз; false дає код 1 і діагностику |
| MinGW-w64 GCC 14.2 Windows x64 CPU Release | Зібрано EXE, тести й утиліти |
| Windows x64 CTest під Wine 10/Xvfb | 22/22; два додаткові Linux-тести тут не реєструються |
| Windows EXE з повного пакета під Wine | CPU smoke-test пройшов: VICTORY, 517 ticks, 48 shots; 3 кадри 799×513 |
| Пакет без assets поряд з EXE | Код 3; ресурси з батьківського checkout не підхоплюються |
| Пакет без glyph advances | Код 4 із повідомленням про невдале завантаження ресурсів |

Нативний запуск на Windows 11, MSVC-збірка й виконання GitHub Actions
у цій Linux-машині не перевірені. Wine не замінює перевірку на Windows.
PowerShell-скрипти додані для Windows-перевірки; їхній запуск тут не виконано.
Нативний Vulkan/GPU-рендеринг залишається неперевіреним.

## Команди

Linux із встановленими CMake, C++20, X11, SDL2 та Xvfb:

```bash
cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DGZ_ENABLE_VULKAN=OFF -DGZ_REQUIRE_VULKAN=OFF
cmake --build build-debug --parallel 4
ctest --test-dir build-debug --output-on-failure --no-tests=error --parallel 4
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DGZ_ENABLE_VULKAN=OFF -DGZ_REQUIRE_VULKAN=OFF
cmake --build build-release --parallel 4
ctest --test-dir build-release --output-on-failure --no-tests=error --parallel 4
```

У підготовленому cloud-середовищі перед цим активувати інструменти:
`source /workspace/.grayzone-env/activate.sh`.

Windows із Visual Studio 2022 C++/CMake: `build_windows_cpu.bat`.
Повторна перевірка готової папки:

```powershell
./scripts/verify_windows_package.ps1 -DistDir dist-win-cpu
```

Ручний smoke-test із папки дистрибутива: `GRAY_ZONE_1989.exe --smoke-test`.
Звичайний запуск: `PLAY.bat`. Результат native Vulkan за CPU-кадрами не оцінювати.

## Наступна перевірка

Запустити Windows CPU-пакет на Windows 11: перевірити меню, керування,
звук, контракт, F5/F9 та перехід на стратегічну карту. Виконати MSVC CI
і переконатися, що перевірка упакованої гри проходить в обох конфігураціях.
Після цього завершувати Vulkan/HUD і спільний цикл Windows/X11.
