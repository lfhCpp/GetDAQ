#!/usr/bin/env python3
"""Check local Markdown file links, excluding fenced and inline code examples."""
from pathlib import Path
import re
from urllib.parse import unquote


def prose(text):
    lines = []
    fence = None
    for line in text.splitlines():
        marker = re.match(r'^ {0,3}(`{3,}|~{3,})(.*)$', line)
        if fence:
            if marker and marker[1][0] == fence[0] and len(marker[1]) >= len(fence) and not marker[2].strip():
                fence = None
        elif marker:
            fence = marker[1]
        else:
            lines.append(line)
    return re.sub(r'(`+)(.*?)\1', '', '\n'.join(lines))


def main():
    root = Path(__file__).resolve().parents[1]
    files = [root / name for name in ['README.md', 'REWRITE_PROPOSAL.md', 'third_party/README.md']]
    files += sorted((root / 'docs').glob('*.md'))
    checked = 0
    for path in files:
        text = prose(path.read_text())
        for hit in re.finditer(r'\]\(', text):
            start = hit.end()
            if text[start:start + 1] == '<':
                end = text.find('>', start + 1)
                assert end >= 0, (path, 'Unclosed link destination')
                target = text[start + 1:end]
            else:
                depth, end = 1, start
                while end < len(text) and depth:
                    if end == 0 or text[end - 1] != '\\':
                        if text[end] == '(':
                            depth += 1
                        elif text[end] == ')':
                            depth -= 1
                    end += 1
                assert depth == 0, (path, 'Unclosed link destination')
                target = text[start:end - 1].split(' "', 1)[0]
            if target.startswith(('http:', 'https:', 'mailto:', '#', 'app:')):
                continue
            target = unquote(target.split('#', 1)[0]).replace('\\(', '(').replace('\\)', ')')
            if target:
                assert (path.parent / target).exists(), (path, target)
                checked += 1
    print('PASS', checked, 'local Markdown file links; code examples excluded; anchors not checked')


if __name__ == '__main__':
    main()
