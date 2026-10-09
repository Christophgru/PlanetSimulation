"""Assemble labelled contact sheets from the actual rendered views."""
import json
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'USER_IO/astronaut_vis'
CACHE = ROOT / 'build-f5/astronaut-next/sheets'


def main():
    CACHE.mkdir(parents=True, exist_ok=True)
    rows = json.loads((OUT / 'sources.json').read_text())['models']
    for view in ('front', 'back', 'detail'):
        panels = []
        for row in rows:
            key = row['key']
            source = OUT / (key + '.png') if view == 'front' else OUT / 'views' / (key + '-' + view + '.png')
            panel = CACHE / (key + '-' + view + '.png')
            label = (row['name'] + '\n' + format(row['triangles'] - row.get('degenerate_triangles', 0), ',')
                     + ' triangles | NO RIG')
            subprocess.run(['convert', str(source), '-resize', '450x540',
                            '-background', '#202a3a', '-fill', 'white',
                            '-font', 'DejaVu-Sans', '-pointsize', '18',
                            '-gravity', 'north', '-splice', '0x64',
                            '-gravity', 'north', '-annotate', '+0+9', label,
                            str(panel)], check=True)
            panels.append(str(panel))
        target = OUT / 'comparison.png' if view == 'front' else OUT / 'views' / (view + '-comparison.png')
        subprocess.run(['convert', *panels, '+append', str(target)], check=True)
        print('Assembled', target.relative_to(ROOT))
    refs = json.loads((OUT / 'references/sources.json').read_text())
    panels = []
    for row in refs:
        panel = CACHE / (row['uid'] + '-reference.png')
        label = row['author'] + '\nOFFICIAL PREVIEW | RIG UNVERIFIED'
        subprocess.run(['convert', str(OUT / 'references' / row['image_file']),
                        '-resize', '450x400', '-background', '#202a3a',
                        '-gravity', 'center', '-extent', '450x400',
                        '-fill', 'white', '-font', 'DejaVu-Sans', '-pointsize', '16',
                        '-gravity', 'north', '-splice', '0x64',
                        '-annotate', '+0+9', label, str(panel)], check=True)
        panels.append(str(panel))
    assert len(panels) == 4
    for index in range(2):
        subprocess.run(['convert', *panels[index * 2:index * 2 + 2], '+append',
                        str(CACHE / ('references-' + str(index) + '.png'))], check=True)
    subprocess.run(['convert', str(CACHE / 'references-0.png'),
                    str(CACHE / 'references-1.png'), '-append',
                    str(OUT / 'references/comparison.png')], check=True)


if __name__ == '__main__':
    main()
