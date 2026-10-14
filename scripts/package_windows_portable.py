"""Package the CPU/Vulkan Windows pair, validate assets, optionally smoke-test it."""
import argparse
import hashlib
from pathlib import Path
import shutil
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parent.parent
SHADERS = ('native_scene.vert', 'native_scene.frag', 'native_shadow.vert',
           'native_shadow.frag', 'native_sky.vert', 'native_sky.frag', 'native_ui.vert', 'native_ui.frag')


def executable(build, name):
    for candidate in (build / name, build / 'Release' / name):
        if candidate.is_file():
            return candidate
    raise FileNotFoundError(f'Windows executable missing: {build / name}')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cpu-build', required=True, type=Path)
    parser.add_argument('--vulkan-build', required=True, type=Path)
    parser.add_argument('--dist', required=True, type=Path)
    parser.add_argument('--zip', required=True, type=Path)
    parser.add_argument('--smoke-runner', type=Path,
                        help='Linux Wine wrapper; on Windows EXEs run directly')
    parser.add_argument('--verify-native', action='store_true')
    parser.add_argument('--evidence-dir', type=Path)
    args = parser.parse_args()
    dest = args.dist.resolve()
    dest.mkdir(parents=True, exist_ok=False)
    shutil.copytree(ROOT / 'assets', dest / 'assets')
    binaries = {'GRAY_ZONE_1989.exe': executable(args.vulkan_build, 'GRAY_ZONE_1989.exe'),
                'GRAY_ZONE_1989_CPU.exe': executable(args.cpu_build, 'GRAY_ZONE_1989.exe'),
                'grayzone_vulkan_probe.exe': executable(args.vulkan_build, 'grayzone_vulkan_probe.exe')}
    for name, source in binaries.items():
        shutil.copy2(source, dest / name)
    shader_dir = args.vulkan_build / 'assets/shaders/vulkan'
    for shader in SHADERS:
        shutil.copy2(shader_dir / (shader + '.spv'), dest / 'assets/shaders/vulkan' / (shader + '.spv'))
    launch = ('@echo off\r\ncd /d "%~dp0"\r\n'
              'if exist "%SystemRoot%\\System32\\vulkan-1.dll" (\r\n'
              '  "%~dp0GRAY_ZONE_1989.exe" %*\r\n) else (\r\n'
              '  "%~dp0GRAY_ZONE_1989_CPU.exe" %*\r\n)\r\n')
    (dest / 'PLAY.bat').write_bytes(launch.encode('ascii'))
    for filename, command in {
        'PLAY_CPU.bat': '"%~dp0GRAY_ZONE_1989_CPU.exe" %*',
        'PLAY_HIGH.bat': 'set GZ_GPU_QUALITY=high\r\ncall "%~dp0PLAY.bat" %*',
        'PLAY_MEDIUM.bat': 'set GZ_GPU_QUALITY=medium\r\ncall "%~dp0PLAY.bat" %*',
        'PLAY_LOW.bat': 'set GZ_GPU_QUALITY=low\r\ncall "%~dp0PLAY.bat" %*',
        'GPU_PROBE.bat': '"%~dp0grayzone_vulkan_probe.exe"\r\npause',
        'VERIFY_GPU.bat': 'call "%~dp0PLAY.bat" --gpu-smoke-test\r\necho Exit code: %errorlevel%\r\npause',
    }.items():
        (dest / filename).write_bytes(('@echo off\r\ncd /d "%~dp0"\r\n' + command + '\r\n').encode('ascii'))
    shutil.copy2(ROOT / 'README_UA.md', dest / 'README_GAME_UA.md')
    for document in ('GPU_OPTIMIZATION_UA.md', 'GRAPHICS_RTX3050TI_UA.md', 'UI_TERRAIN_v024_UA.md'):
        if (ROOT / 'docs' / document).is_file():
            shutil.copy2(ROOT / 'docs' / document, dest / document)
    (dest / 'README_UA.txt').write_text('''GRAY ZONE: 1989 — v0.0.24, Windows x64

Повністю розпакуйте архів. Запуск: PLAY.bat або PLAY_HIGH.bat.
Для RTX 3050 Ti 4 ГБ почніть із високої якості. Якщо гра працює повільно,
спробуйте PLAY_MEDIUM.bat або PLAY_LOW.bat. CPU-запуск: PLAY_CPU.bat.
Папка assets має залишатися поруч з EXE; не запускайте гру всередині ZIP.
Для Vulkan потрібен актуальний драйвер NVIDIA/AMD/Intel; SDK не потрібний.

Нові текстури, об'ємні бійці зі зброєю, танк, БТР і вантажівка, деталізовані
будинки й дерева, тіні, MSAA, вода, погода та освітлення працюють у Vulkan.
Техніка є декоративними об'єктами сцени; водіння й бойова AI техніки
не додавалися. CPU-рендерер зберігає свої попередні ефекти й моделі.

Висока якість: 512px матеріали з mip-рівнями, тіні 2048px, MSAA до 4x.
Середня: тіні 1024px, MSAA до 4x. Низька: тіні 512px, без MSAA.
F2 — час доби; F3 — погода; F4 — тіні; F7 — вітер; F8 — відбиття води.
Клавіша 2 — наближений тактичний режим. Інше керування: README_GAME_UA.md.

VERIFY_GPU.bat перевіряє native Vulkan без дозволу на CPU fallback
і записує gpu_smoke_test.log; graphics_backend.log містить стан рендерера.
GPU_PROBE.bat перевіряє тільки loader/драйвер.

Новий HUD, seed-рельєф і макротекстури: UI_TERRAIN_v024_UA.md.
Попередня графічна база: GRAPHICS_RTX3050TI_UA.md.
У цьому середовищі немає фізичної RTX 3050 Ti чи Windows 11;
частота кадрів на вашому ПК ще не виміряна.
''', encoding='utf-8-sig')
    for source in (ROOT / 'assets').rglob('*'):
        if source.is_file():
            target = dest / 'assets' / source.relative_to(ROOT / 'assets')
            if hashlib.sha256(source.read_bytes()).digest() != hashlib.sha256(target.read_bytes()).digest():
                raise ValueError(f'Packaged asset differs: {source}')
    evidence = dest / 'verification'
    evidence.mkdir()
    if args.evidence_dir:
        for source in args.evidence_dir.glob('*.log'):
            if not source.name.startswith(('graphics-', 'v024-')):
                continue
            if 'tests' in source.name or 'gallery' in source.name or 'smoke' in source.name:
                shutil.copy2(source, evidence / source.name)
    if args.smoke_runner or __import__('os').name == 'nt':
        runner = [str(args.smoke_runner.resolve())] if args.smoke_runner else []
        for name in ('GRAY_ZONE_1989.exe', 'GRAY_ZONE_1989_CPU.exe'):
            with (evidence / (name + '-cpu-startup.log')).open('wb') as output:
                subprocess.run(runner + [str(dest / name), '--smoke-test'], cwd=dest,
                               stdout=output, stderr=subprocess.STDOUT, timeout=60, check=True)
            if 'CPU PACKAGE SMOKE: PASS' not in (dest / 'smoke_test.log').read_text():
                raise ValueError(f'CPU smoke did not pass: {name}')
            shutil.copy2(dest / 'smoke_test.log', evidence / (name + '-smoke.log'))
        if args.verify_native:
            with (evidence / 'windows-native-smoke.log').open('wb') as output:
                subprocess.run(runner + [str(dest / 'GRAY_ZONE_1989.exe'), '--gpu-smoke-test'], cwd=dest,
                               stdout=output, stderr=subprocess.STDOUT, timeout=60, check=True)
            if 'GPU PACKAGE SMOKE: PASS' not in (dest / 'gpu_smoke_test.log').read_text():
                raise ValueError('Native Vulkan startup did not pass')
            shutil.copy2(dest / 'gpu_smoke_test.log', evidence / 'windows-native-smoke-result.log')
        for source in list(dest.glob('*.log')) + list(dest.glob('test_build_*.ppm')):
            source.unlink()
    commit = subprocess.run(['git', 'rev-parse', 'HEAD'], cwd=ROOT, capture_output=True,
                            text=True, check=True).stdout.strip()
    (dest / 'SOURCE_COMMIT.txt').write_text(commit + '\n', encoding='ascii')
    manifest = ''.join(hashlib.sha256(p.read_bytes()).hexdigest() + '  ' + p.relative_to(dest).as_posix() + '\n'
                       for p in sorted(dest.rglob('*')) if p.is_file())
    (dest / 'SHA256SUMS.txt').write_text(manifest, encoding='utf-8')
    with zipfile.ZipFile(args.zip, 'w', zipfile.ZIP_DEFLATED, compresslevel=6) as archive:
        for source in sorted(dest.rglob('*')):
            if source.is_file():
                archive.write(source, Path('GRAY_ZONE_1989') / source.relative_to(dest))
    with zipfile.ZipFile(args.zip) as archive:
        if archive.testzip() is not None:
            raise ValueError('ZIP CRC validation failed')
    digest = hashlib.sha256(args.zip.read_bytes()).hexdigest()
    Path(str(args.zip) + '.sha256').write_text(digest + '  ' + args.zip.name + '\n', encoding='ascii')
    print(f'Packaged {args.zip} ({args.zip.stat().st_size} bytes); SHA256 {digest}')


if __name__ == '__main__':
    main()
