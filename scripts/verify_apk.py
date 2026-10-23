#!/usr/bin/env python3
"""Reject mixed ABI APKs and unresolved direct/transitive native dependencies."""
import hashlib
from pathlib import Path
import re
import struct
import subprocess
import sys
import tempfile
import zipfile


def audit(apk, readelf, system_dir):
    system = {p.name for p in Path(system_dir).glob('*.so')}
    if not {'libc.so', 'libm.so', 'libdl.so'}.issubset(system):
        raise ValueError('NDK API stub library inventory is absent')
    with zipfile.ZipFile(apk) as z, tempfile.TemporaryDirectory() as tmp:
        if z.testzip() is not None:
            raise ValueError('Corrupt ZIP entry')
        for name in ('AndroidManifest.xml', 'classes.dex', 'resources.arsc'):
            if name not in z.namelist():
                raise ValueError(f'Missing {name}')
        if 'assets/data/codeeditor/compilers/default.xml' not in z.namelist():
            raise ValueError('Missing upstream runtime data assets')
        libs = [n for n in z.namelist() if n.startswith('lib/') and n.endswith('.so')]
        if not libs:
            raise ValueError('APK has no native libraries')
        available = {Path(n).name for n in libs}
        required = ['libsimulide', 'libQt5Core_', 'libQt5Gui_', 'libQt5Widgets_',
                    'libQt5Svg_', 'libQt5Multimedia_', 'libQt5SerialPort_',
                    'libplugins_platforms_qtforandroid_']
        for prefix in required:
            if not any(n.startswith(prefix) for n in available):
                raise ValueError(f'Missing runtime library: {prefix}')
        if 'libc++_shared.so' not in available:
            raise ValueError('Missing Android C++ runtime')
        for name in libs:
            if not name.startswith('lib/arm64-v8a/'):
                raise ValueError(f'Unexpected ABI: {name}')
            data = z.read(name)
            if len(data) < 64 or data[:4] != b'\x7fELF' or data[4:6] != b'\x02\x01' or struct.unpack_from('<H', data, 18)[0] != 183:
                raise ValueError(f'Not little-endian ELF64 AArch64: {name}')
            path = Path(tmp) / Path(name).name
            path.write_bytes(data)
            dynamic = subprocess.check_output([readelf, '-d', str(path)], text=True)
            needed = re.findall(r'\(NEEDED\).*?\[(.*?)\]', dynamic)
            missing = set(needed) - available - system
            if missing:
                raise ValueError(f'{name}: unresolved dependencies {sorted(missing)}')
            print(f'PASS {Path(name).name}: {", ".join(needed)}')
        print(f'PASS: {len(libs)} ARM64 libraries, manifest, DEX, resources, dependency closure')
        print(f'SHA256 {hashlib.sha256(Path(apk).read_bytes()).hexdigest()}')


if __name__ == '__main__':
    try:
        audit(*sys.argv[1:])
    except (ValueError, zipfile.BadZipFile) as error:
        raise SystemExit(f'APK AUDIT FAILED: {error}')
