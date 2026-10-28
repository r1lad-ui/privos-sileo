#!/usr/bin/env python3
"""Render editable PRIV OS SVG icons to installable 512 × 512 PNGs.

Requires Python 3, Pillow and either CairoSVG or Inkscape. The SVG files are
self-contained; this script does not download anything and does not access
the iPhone. It writes only assets/icons/png and the requested preview path.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import shutil
import subprocess
import tempfile

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]


def render(source: Path) -> Path:
    destination = ROOT / 'assets/icons/png' / (source.stem + '.png')
    destination.parent.mkdir(parents=True, exist_ok=True)
    temporary_file = tempfile.NamedTemporaryFile(prefix='.render-', suffix='.png',
                                                 dir=destination.parent, delete=False)
    temporary_file.close()
    temporary = Path(temporary_file.name)
    try:
        import cairosvg
    except ImportError:
        executable = shutil.which('inkscape')
        if not executable:
            raise SystemExit('Install CairoSVG or Inkscape to render the SVG icons.')
        subprocess.run([executable, '--batch-process', str(source), '--export-type=png',
                        f'--export-filename={temporary}', '--export-width=512',
                        '--export-height=512'], check=True, capture_output=True)
    else:
        cairosvg.svg2png(url=str(source), write_to=str(temporary),
                       output_width=512, output_height=512)
    with Image.open(temporary) as image:
        if image.size != (512, 512):
            raise ValueError(f'Unexpected output size: {temporary}')
        normalized = image.convert('RGBA')
    normalized.save(temporary, format='PNG', optimize=True)
    with Image.open(temporary) as image:
        image.verify()
    temporary.replace(destination)
    return destination


def preview(destination: Path) -> None:
    manifest = json.loads((ROOT / 'assets/icons/manifest-design.json').read_text())
    icons = manifest['icons']
    columns = 6
    cell_w, cell_h = 210, 235
    rows = (len(icons) + columns - 1) // columns
    sheet = Image.new('RGB', (columns * cell_w + 60, rows * cell_h + 130), '#0a1722')
    draw = ImageDraw.Draw(sheet)
    font_file = '/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf'
    font = ImageFont.truetype(font_file, 18) if Path(font_file).exists() else ImageFont.load_default()
    title = ImageFont.truetype(font_file, 29) if Path(font_file).exists() else font
    draw.text((30, 23), 'PRIV OS · Icônes complémentaires', font=title, fill='#e1f0f8')
    for i, icon in enumerate(icons):
        x, y = 30 + (i % columns) * cell_w, 88 + (i // columns) * cell_h
        with Image.open(ROOT / 'assets/icons/png' / (icon['slug'] + '.png')) as image:
            image = image.convert('RGB').resize((178, 178), Image.Resampling.LANCZOS)
            sheet.paste(image, (x + 16, y))
        text = icon['label']
        if draw.textlength(text, font=font) > cell_w - 4:
            text = text.replace(' ', '\n', 1)
        draw.multiline_text((x + cell_w / 2, y + 189), text, font=font,
                            fill='#d3e5ed', anchor='ma', align='center', spacing=4)
    destination.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(destination)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--preview', type=Path)
    args = parser.parse_args()
    sources = sorted((ROOT / 'assets/icons/svg').glob('*.svg'))
    results = [render(source) for source in sources]
    if args.preview:
        preview(args.preview)
    print(f'Rendered {len(results)} icons at 512 × 512 pixels.')


if __name__ == '__main__':
    main()
