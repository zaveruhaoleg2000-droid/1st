GRAY ZONE: 1989 v0.0.24 — Windows x64 prototype

Додано native HUD із картками загонів/мінікартою, picking за актуальною камерою, seed-генерацію рельєфу й фонову macro map 4096/2048/1024. Старі campaign saves підтримуються.

Повністю розпакуйте GRAY_ZONE_1989_v024_WIN_X64.zip і запустіть PLAY_HIGH.bat. CPU fallback: PLAY_CPU.bat. Для Vulkan потрібен актуальний відеодрайвер.

Повний український огляд з шістьма справжніми скріншотами й розбором механік: GRAY_ZONE_1989_v024_REVIEW_UA.html. Зображення вбудовані, файл відкривається офлайн у браузері. Тактичні кадри — native Vulkan через програмний Mesa Lavapipe; меню/карта — CPU Windows EXE під Wine.

Перевірено Linux CPU Debug/Release 27/27, Vulkan 28/28 із synchronization validation, Windows CPU/Vulkan 25/25 під Wine. Упаковані EXE пройшли CPU та native Vulkan smoke (HUD, macro 4096, 16 materials, shadow 2048, MSAA 4). Фізична RTX 3050 Ti й Windows 11 недоступні; FPS на цільовому ПК не виміряно.

Техніка декоративна. Collision/navigation, LOS/укриття та різні бойові шаблони місій ще не реалізовані.

Source branch: graphics/v0.0.24
Source commit: 6e272017501fd837d797de2fff6d9326d9736832
