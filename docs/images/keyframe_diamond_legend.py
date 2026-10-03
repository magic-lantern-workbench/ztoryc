#!/usr/bin/env python3
"""Generates keyframe_diamond_legend.svg — the same four regions and colours as
include/toonzqt/ztorykeydiamond.h (keyDiamond). Regenerate after any change
there: python3 docs/images/keyframe_diamond_legend.py"""
import os

W, G, O = '#FFFFFF', '#ECA61C', None
ROWS = [
    ((W, W, W, W), 'Transform key', 'The column transform is keyed on every channel.'),
    ((W, W, O, O), 'Partial transform key', 'Some transform channels are keyed, not all.'),
    ((W, W, G, G), 'Full key — transform + pose', 'Everything is keyed: the whole character is locked here.'),
    ((G, G, G, G), 'Pose key', 'The plastic pose is fully keyed, the transform not at all.'),
    ((G, G, W, O), 'Full pose, partial transform', 'Global Key scope Plastic: the pose plus the channel you moved.'),
    ((W, W, G, O), 'Full transform, partial pose', 'Global Key scope Stage: the transform plus the joints you moved.'),
    ((W, G, O, O), 'Partial — transform + pose', 'Both systems hold a key, neither is complete.'),
    ((G, G, O, O), 'Partial pose key', 'Some joints are keyed, the transform not at all.'),
]

def diamond(cx, cy, r, regions, outline='#1a1a1a'):
    lt, lb, rt, rb = regions
    top, right, bottom, left = (cx, cy - r), (cx + r, cy), (cx, cy + r), (cx - r, cy)
    mid = (cx, cy)
    tris = [
        (lt, [top, mid, left]),
        (lb, [mid, bottom, left]),
        (rt, [top, right, mid]),
        (rb, [mid, right, bottom]),
    ]
    out = []
    for col, pts in tris:
        if col:
            p = ' '.join('%.1f,%.1f' % q for q in pts)
            out.append('  <polygon points="%s" fill="%s" stroke="%s" stroke-width="0.8"/>' % (p, col, col))
    p = ' '.join('%.1f,%.1f' % q for q in [top, right, bottom, left])
    out.append('  <polygon points="%s" fill="none" stroke="%s" stroke-width="1.2"/>' % (p, outline))
    return '\n'.join(out)

def text(x, y, s, size, color, bold=False):
    s = s.replace('&', '&amp;').replace('<', '&lt;')
    w = ' font-weight="600"' if bold else ''
    return '<text x="%d" y="%d" fill="%s" font-size="%s"%s>%s</text>' % (x, y, color, size, w, s)

rowH, top0 = 62, 96
footY = top0 + len(ROWS) * rowH + 30
H = footY + 62
L = ['<svg xmlns="http://www.w3.org/2000/svg" width="760" height="%d" viewBox="0 0 760 %d" '
     'font-family="-apple-system,Segoe UI,Roboto,sans-serif">' % (H, H),
     '<rect width="760" height="%d" rx="10" fill="#2b2b2b"/>' % H,
     text(28, 42, 'Keyframe diamond — how to read it', 19, '#e8e8e8', True),
     text(28, 66, 'Left half = which system (white transform, gold pose) · '
                  'an empty quarter on the right = something is partial', 13, '#a8a8a8')]
for i, (reg, title, desc) in enumerate(ROWS):
    y = top0 + i * rowH
    L.append('<rect x="20" y="%d" width="720" height="54" rx="6" fill="#3a3a3a"/>' % y)
    L.append(diamond(56, y + 27, 13, reg))
    L.append(text(92, y + 23, title, 14, '#e8e8e8', True))
    L.append(text(92, y + 41, desc, 12.5, '#a8a8a8'))
L.append(text(36, footY, 'Same grammar in the viewer’s Set Key button', 13, '#e8e8e8', True))
fy = footY + 18
L.append(diamond(53, fy, 9, (O, O, O, O), '#969696'))
L.append(text(74, fy + 5, 'no key', 12, '#a8a8a8'))
L.append('<rect x="168" y="%d" width="26" height="26" rx="3" fill="#B01E9A"/>' % (fy - 13))
L.append(diamond(181, fy, 9, (W, W, O, O)))
L.append(text(202, fy + 5, 'partial', 12, '#a8a8a8'))
L.append('<rect x="296" y="%d" width="26" height="26" rx="3" fill="#B01E9A"/>' % (fy - 13))
L.append(diamond(309, fy, 9, (W, W, G, G)))
L.append(text(330, fy + 5, 'full', 12, '#a8a8a8'))
L.append(text(430, fy + 5, '— magenta background = this frame holds a key', 12, '#a8a8a8'))
L.append('</svg>')
out = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'keyframe_diamond_legend.svg')
open(out, 'w', encoding='utf-8').write('\n'.join(L) + '\n')
print('written', out)
