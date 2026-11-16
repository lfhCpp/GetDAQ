#!/usr/bin/env python3
"""Prepare original cconfiged using only the curated project-local source bundle."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import sys

ROOT = Path(__file__).resolve().parents[1]
VENDOR = ROOT / 'third_party/legacy-get'


def prepare(destination):
    destination.mkdir(parents=True, exist_ok=True)
    packages = {
        'widgets': VENDOR / 'widgets',
        'compound': VENDOR / 'compound',
        'tinyxml': VENDOR / 'tinyxml',
    }
    inputs = {}
    for name, original in packages.items():
        if not (original / 'src').is_dir():
            raise RuntimeError('Original cconfig sources missing: ' + str(original))
        for path in original.rglob('*'):
            if not path.is_file() or '.svn' in path.parts:
                continue
            relative = path.relative_to(original)
            if relative.parts[0] not in ('src', 'res') and path.name != 'COPYING':
                continue
            target = destination / name / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            data = path.read_bytes()
            inputs[str(path.relative_to(ROOT))] = hashlib.sha256(data).hexdigest()
            # qmake generates ui_Foo.h rather than autotools' Foo.ui.h.
            if name == 'widgets' and path.suffix in ('.cpp', '.h', '.hpp'):
                text = data.decode('utf-8')
                text = re.sub(r'["<]CCfg/Ui/(\w+)\.ui\.h[">]', r'"ui_\1.h"', text)
                data = text.encode('utf-8')
            if not target.exists() or target.read_bytes() != data:
                target.write_bytes(data)
    utility = VENDOR / 'utilities/src/utl'
    for name in ('Logging.h', 'Logging.cpp'):
        path = utility / name
        data = path.read_bytes()
        inputs[str(path.relative_to(ROOT))] = hashlib.sha256(data).hexdigest()
        target = destination / 'utilities/utl' / name
        target.parent.mkdir(parents=True, exist_ok=True)
        if not target.exists() or target.read_bytes() != data:
            target.write_bytes(data)
    # Curated headers and required compiled Boost sources live in the project.
    for path in (VENDOR / 'boost_1_55_0').rglob('*'):
        if not path.is_file():continue
        data=path.read_bytes();inputs[str(path.relative_to(ROOT))]=hashlib.sha256(data).hexdigest()
        target=destination/'boost_1_55_0'/path.relative_to(VENDOR/'boost_1_55_0')
        target.parent.mkdir(parents=True,exist_ok=True)
        if not target.exists() or target.read_bytes()!=data:target.write_bytes(data)
    (destination / 'widgets/src/config.h').write_text('#define VERSION "2.1.0 (GetDAQ bundled original)"\n')
    sources = []
    for name in ('compound', 'widgets'):
        src = destination / name / 'src'
        text = (src / 'Makefile.am').read_text()
        if name == 'compound':
            blocks = [text.split('\nsources = ', 1)[1].split('\n\nAM_CXXFLAGS', 1)[0]]
            for section in ('BUILD_CCFG_FMT_XML', 'BUILD_CCFG_FMT_BINARY'):
                blocks.append(text.split('if ' + section, 1)[1].split('endif', 1)[0].split('sources +=', 1)[1])
            sources.extend(src / p for p in re.findall(r'[\w/]+\.cpp', '\n'.join(blocks)))
        else:
            block = text.split('\nsources = ', 1)[1].split('\nmoc_sources', 1)[0]
            sources.extend(src / p for p in re.findall(r'[\w/]+\.cpp', block))
            sources.append(src / 'main.cpp')
    sources += [destination / 'tinyxml/src' / p for p in ('tinyxml.cpp', 'tinystr.cpp', 'tinyxmlparser.cpp', 'tinyxmlerror.cpp')]
    sources.append(destination / 'utilities/utl/Logging.cpp')
    # Both packages define Exception.cpp; keep distinct qmake object filenames.
    duplicate = destination / 'compound/src/CCfg/Exception.cpp'
    distinct = duplicate.with_name('CompoundException.cpp')
    shutil.copyfile(duplicate, distinct)
    sources = [distinct if p == duplicate else p for p in sources]
    assert len(sources) == len(set(sources)) and all(p.is_file() for p in sources)
    def q(path):
        return '"' + str(path).replace('\\', '/') + '"'
    lines = ['INCLUDEPATH += ' + ' '.join(q(destination / p) for p in
             ('widgets/src', 'compound/src', 'tinyxml/src', 'utilities', 'boost_1_55_0'))]
    lines += ['SOURCES += ' + q(p) for p in sources]
    lines += ['HEADERS += ' + q(p) for p in (destination / 'widgets/src').rglob('*.h')]
    lines += ['FORMS += ' + q(p) for p in (destination / 'widgets/src').rglob('*.ui')]
    lines += ['RESOURCES += ' + q(destination / 'widgets/src/ConfigWidgets.qrc')]
    (destination / 'original.pri').write_text('\n'.join(lines) + '\n')
    (destination / 'original-inputs.json').write_text(json.dumps(inputs, indent=2) + '\n')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--destination', type=Path, required=True)
    args = parser.parse_args()
    try:
        prepare(args.destination.resolve())
    except (OSError, RuntimeError, AssertionError) as error:
        sys.exit(str(error))
