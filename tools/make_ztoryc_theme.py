#!/usr/bin/env python3
"""Generate stuff/config/qss/Ztoryc/Ztoryc.qss from Tahoma's Dark.qss.

The Ztoryc palette (Franco, 2026-10-19):
  - neutrals: Dark's greys, tinted a little warm (graphite, hue 30);
  - gold  = active / where you are (checked tools, current room, checkboxes);
  - blue  = selection (menus, lists, text, cells);
  - red   = current frame (xsheet current row, like the animatic playhead);
  - Tahoma's magenta is gone.
Rerun after changing Dark.qss:  python3 tools/make_ztoryc_theme.py
"""
import colorsys, os, re, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, 'stuff/config/qss/Dark/Dark.qss')
DST = os.path.join(ROOT, 'stuff/config/qss/Ztoryc/Ztoryc.qss')

GOLD, GOLD_BG, GOLD_BG_HOVER = '#f5b800', '#6b5210', '#7d6114'
BLUE, BLUE_HOVER, BLUE_FOCUS = '#2f6fc9', '#3d7fdb', '#4a9eff'


def warm(v):
    """Grey level v (0-255) -> warm graphite. Must match ZtoryTheme::warm()."""
    if v in (0, 255):
        return (v, v, v)
    # Saturation by band: in HLS the tint shrinks towards white, so the light
    # text greys need more of it to read as warm at all.
    s = 0.12 if v < 128 else (0.08 if v < 204 else 0.25)
    r, g, b = colorsys.hls_to_rgb(30 / 360, v / 255, s)
    return tuple(round(c * 255) for c in (r, g, b))


def hexcol(t):
    return '#%02x%02x%02x' % t


def warm_hex(m):
    h = m.group(1)
    if len(h) == 3:
        h = ''.join(c * 2 for c in h)
    r, g, b = int(h[0:2], 16), int(h[2:4], 16), int(h[4:6], 16)
    if r == g == b:
        return hexcol(warm(r))
    return m.group(0)


def warm_rgba(m):
    r, g, b = int(m.group(1)), int(m.group(2)), int(m.group(3))
    if r == g == b:
        wr, wg, wb = warm(r)
        return 'rgba(%d, %d, %d, %s)' % (wr, wg, wb, m.group(4))
    return m.group(0)


# (selector fragment, property, new value). The selector fragment is matched
# against the rule's header so the same magenta can go gold or blue.
RULES = [
    (':disabled:checked', 'background-color', 'rgba(107, 82, 16, 0.4)'),
    (':disabled:checked', 'border-color', 'rgba(245, 184, 0, 0.4)'),
    (':checked:hover', 'background-color', GOLD_BG_HOVER),
    (':checked:hover', 'border-color', GOLD),
    ('QToolButton:checked', 'background-color', GOLD_BG),
    ('QToolButton:checked', 'border-color', GOLD),
    ('QMenuBar::item:pressed', 'background-color', BLUE),
    ('#TopBarTab::tab:selected', 'background-color', '#3a332c'),
    ('QMenu::item:selected', 'background-color', BLUE),
    ('QMenu::item:checked:selected', 'background-color', BLUE),
    ('QMenu::icon:checked', 'background-color', GOLD_BG),
    ('QMenu::icon:checked', 'border', '1 solid ' + GOLD),
    ('TPanelTitleBarButton', 'qproperty-PressedColor', GOLD_BG),
    ('QTreeView::item:selected', 'background-color', BLUE),
    ('QComboBox:editable', 'selection-background-color', BLUE),
    ('QComboBox:checked', 'border-color', BLUE_FOCUS),
    ('QComboBox:focus', 'border-color', BLUE_FOCUS),
    ('QComboBox QAbstractItemView', 'selection-background-color', BLUE),
    ('QLineEdit,', 'selection-background-color', BLUE),
    ('QLineEdit:focus', 'border-color', BLUE_FOCUS),
    ('QCheckBox::indicator:checked', 'background-color', GOLD_BG),
    ('QCheckBox::indicator:checked', 'border-color', GOLD),
    ('QCheckBox::indicator:indeterminate', 'background-color', GOLD_BG),
    ('QCheckBox::indicator:indeterminate', 'border-color', GOLD),
    ('DvItemViewerPanel', 'qproperty-SelectedItemBackground', BLUE),
    ('StyledTreeView', 'qproperty-SelectedItemBackground', BLUE),
    ('#MatchLineButton:checked', 'border', '2 solid ' + GOLD),
    ('#PageViewer', 'qproperty-SelectedCellColor', BLUE),
    ('#PageViewer', 'qproperty-CurrentCellColor', 'rgba(245, 184, 0, 0.5)'),
    ('QPushButton:focus:hover', 'background-color', BLUE_HOVER),
    ('QPushButton:focus', 'background-color', BLUE),
    ('QListWidget::item:selected', 'background-color', BLUE),
    ('XsheetViewer', 'qproperty-SelectedEmptyCellColor', 'rgba(47, 111, 201, 0.35)'),
    ('XsheetViewer', 'qproperty-SelectedColumnHead', 'rgba(235, 229, 220, 0.3)'),
    ('XsheetViewer', 'qproperty-CurrentRowBgColor', 'rgba(255, 77, 58, 0.45)'),
    ('SpreadsheetViewer', 'qproperty-SelectedEmptyColor', 'rgba(47, 111, 201, 0.35)'),
    ('SpreadsheetViewer', 'qproperty-SelectedSceneRangeEmptyColor', 'rgba(47, 111, 201, 0.35)'),
    ('SpreadsheetViewer', 'qproperty-CurrentRowBgColor', 'rgba(255, 77, 58, 0.45)'),
    ('#MainWindowMessageLabel', 'color', GOLD),
    ('#GraphAreaWidget', 'qproperty-SelectedPointColor', BLUE),
    ('MotionPathPanel', 'qproperty-SelectedColor', BLUE),
]
EXTRA_TOPBAR = '  color: %s;\n  border-bottom: 2 solid %s;\n' % (GOLD, GOLD)


def main():
    text = open(SRC).read()
    # Split into (header, body) rules; comments and blank space pass through.
    out, used, unmapped = [], set(), []
    pos = 0
    for m in re.finditer(r'([^{}]*)\{([^{}]*)\}', text):
        header, body = m.group(1), m.group(2)
        hdr = ' '.join(header.split())
        lines = body.split('\n')
        for i, line in enumerate(lines):
            pm = re.match(r'\s*([\w-]+)\s*:\s*(.*?);\s*$', line)
            if not pm:
                continue
            prop, val = pm.group(1), pm.group(2)
            if not re.search(r'#a35293|#b46aa5|163, 82, 147|106, 89, 102|237, 220, 233', val, re.I):
                continue
            for k, (frag, p, new) in enumerate(RULES):
                if p == prop and frag in hdr:
                    lines[i] = '  %s: %s;' % (prop, new)
                    used.add(k)
                    break
            else:
                unmapped.append('%s :: %s' % (hdr[-70:], line.strip()))
        body = '\n'.join(lines)
        if hdr.endswith('#TopBarTab::tab:selected'):
            body = body.rstrip(' ') + EXTRA_TOPBAR
        out.append(text[pos:m.start()] + header + '{' + body + '}')
        pos = m.end()
    out.append(text[pos:])
    if unmapped:
        sys.exit('UNMAPPED accents:\n' + '\n'.join(unmapped))
    res = ''.join(out)
    res = re.sub(r'#([0-9a-fA-F]{6}|[0-9a-fA-F]{3})\b', warm_hex, res)
    res = re.sub(r'rgba\(\s*(\d+),\s*(\d+),\s*(\d+),\s*([\d.]+)\s*\)', warm_rgba, res)
    leftover = re.findall(r'#a35293|#b46aa5|163, 82, 147', res, re.I)
    if leftover:
        sys.exit('magenta left: %r' % leftover)
    unused = [RULES[k] for k in range(len(RULES)) if k not in used]
    if unused:
        print('note: unused rules', unused)
    os.makedirs(os.path.dirname(DST), exist_ok=True)
    head = ('/* Ztoryc - dark theme. GENERATED from Dark/Dark.qss by\n'
            '   tools/make_ztoryc_theme.py: edit the script, not this file. */\n')
    open(DST, 'w').write(head + res)
    print('written', DST)


if __name__ == '__main__':
    main()
