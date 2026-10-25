#!/usr/bin/env python3
"""
qvedit - the Quake Visuals editor of Q3Doom

Shows Doom's wall textures, flats, skies and items next to the Quake 3
shaders, textures and models that replace them in Quake Visuals
(r_quakeVisuals 1), and writes quakevisuals.dat. It runs a small web page on
this computer: start it, and your browser opens the editor.

    python qvedit.py [--game <folder>] [--port 8765] [--no-browser]

<folder> is the game folder: the one holding baseq3/ (DOOM.WAD and the
pak*.pk3) and q3doom/. By default it is found next to this script
(../../run in the source tree) or in the current folder.

The edited file is saved to <folder>/q3doom/quakevisuals.dat, where the game
reads it (vid_restart reloads it), and in the source tree also to
data/q3doom/quakevisuals.dat, which a build copies over the run folder.

Only the Python standard library is used. Nothing of the game data leaves
this computer: the page is served on 127.0.0.1.

Licensed under the GNU General Public License version 2 or later.
"""

import argparse
import fnmatch
import http.server
import io
import json
import os
import struct
import sys
import threading
import urllib.parse
import webbrowser
import zipfile
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))

# ----------------------------------------------------------------------------
# PNG
# ----------------------------------------------------------------------------

def png_encode(width, height, rgba):
    """RGBA bytes (width * height * 4) to a PNG file"""
    def chunk(kind, data):
        c = struct.pack('>I', len(data)) + kind + data
        return c + struct.pack('>I', zlib.crc32(kind + data) & 0xffffffff)
    stride = width * 4
    raw = b''.join(b'\0' + rgba[y * stride:(y + 1) * stride] for y in range(height))
    return (b'\x89PNG\r\n\x1a\n'
            + chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 6, 0, 0, 0))
            + chunk(b'IDAT', zlib.compress(raw, 6))
            + chunk(b'IEND', b''))

# ----------------------------------------------------------------------------
# DOOM.WAD
# ----------------------------------------------------------------------------

class Wad:
    def __init__(self, path):
        with open(path, 'rb') as f:
            self.data = f.read()
        ident, count, offset = struct.unpack_from('<4sii', self.data, 0)
        if ident not in (b'IWAD', b'PWAD'):
            raise ValueError('%s is not a WAD' % path)
        self.lumps = []
        for i in range(count):
            pos, size, name = struct.unpack_from('<ii8s', self.data, offset + i * 16)
            self.lumps.append((name.rstrip(b'\0').decode('ascii', 'replace').upper(), pos, size))
        self.index = {}
        for i, (name, _, _) in enumerate(self.lumps):
            self.index[name] = i		# the last one wins, as in Doom
        self.palette = self.lump('PLAYPAL')[:768]
        self.flats = self.namespace('F_START', 'F_END', 'FF_START', 'FF_END')
        self.sprites = self.namespace('S_START', 'S_END', 'SS_START', 'SS_END')
        self.textures = self.read_textures()

    def lump(self, name):
        i = self.index.get(name.upper())
        if i is None:
            return None
        _, pos, size = self.lumps[i]
        return self.data[pos:pos + size]

    def namespace(self, *markers):
        """lumps between the start and end markers: name -> data"""
        found, inside = {}, False
        for name, pos, size in self.lumps:
            if name in markers[0::2]:
                inside = True
            elif name in markers[1::2]:
                inside = False
            elif inside and size:
                found[name] = self.data[pos:pos + size]
        return found

    def read_textures(self):
        pnames = self.lump('PNAMES')
        names = []
        if pnames:
            n = struct.unpack_from('<i', pnames, 0)[0]
            names = [pnames[4 + i * 8:12 + i * 8].rstrip(b'\0').decode('ascii', 'replace').upper() for i in range(n)]
        textures = {}
        for lumpname in ('TEXTURE1', 'TEXTURE2'):
            data = self.lump(lumpname)
            if not data:
                continue
            n = struct.unpack_from('<i', data, 0)[0]
            for i in range(n):
                off = struct.unpack_from('<i', data, 4 + i * 4)[0]
                name, _, width, height, _, count = struct.unpack_from('<8siHHih', data, off)
                patches = []
                for p in range(count):
                    ox, oy, patch, _, _ = struct.unpack_from('<hhhhh', data, off + 22 + p * 10)
                    if 0 <= patch < len(names):
                        patches.append((ox, oy, names[patch]))
                textures[name.rstrip(b'\0').decode('ascii', 'replace').upper()] = (width, height, patches)
        return textures

    def draw_patch(self, data, image, width, height, ox, oy, mirror=False):
        """a picture in Doom's column format onto an RGBA bytearray"""
        pw, ph = struct.unpack_from('<hh', data, 0)
        pal = self.palette
        for x in range(pw):
            col = struct.unpack_from('<i', data, 8 + x * 4)[0]
            dx = ox + ( pw - 1 - x if mirror else x )
            if dx < 0 or dx >= width:
                continue
            top = -1
            while col < len(data):
                rowstart = data[col]
                if rowstart == 255:
                    break
                # tall patches (DeePsea): a start at or below the last one adds to it
                top = top + rowstart if rowstart <= top else rowstart
                length = data[col + 1]
                for j in range(length):
                    y = oy + top + j
                    if 0 <= y < height:
                        c = data[col + 3 + j] * 3
                        o = (y * width + dx) * 4
                        image[o:o + 4] = bytes((pal[c], pal[c + 1], pal[c + 2], 255))
                col += length + 4

    def picture(self, data):
        w, h = struct.unpack_from('<hh', data, 0)
        image = bytearray(w * h * 4)
        self.draw_patch(data, image, w, h, 0, 0)
        return w, h, bytes(image)

    def texture_png(self, name):
        tex = self.textures.get(name.upper())
        if not tex:
            return None
        width, height, patches = tex
        image = bytearray(width * height * 4)
        for ox, oy, pname in patches:
            data = self.lump(pname)
            if data:
                self.draw_patch(data, image, width, height, ox, oy)
        return png_encode(width, height, bytes(image))

    def flat_png(self, name):
        data = self.flats.get(name.upper())
        if not data or len(data) < 4096:
            return None
        pal = self.palette
        image = bytearray(64 * 64 * 4)
        for i in range(4096):
            c = data[i] * 3
            image[i * 4:i * 4 + 4] = bytes((pal[c], pal[c + 1], pal[c + 2], 255))
        return png_encode(64, 64, bytes(image))

    def sprite_png(self, name):
        name = name.upper()
        data = self.sprites.get(name)
        if not data:
            # a frame stored as the second half of a mirrored pair
            for lump, d in self.sprites.items():
                if len(lump) == 8 and lump[4:] == name[4:6] + name[4:6] and lump.startswith(name[:4]):
                    data = d
            if not data:
                return None
        return png_encode(*self.picture(data))

# Doom's items: thing number, sprite, name
DOOM_ITEMS = [
    (2001, 'SHOTA0', 'Shotgun'), (2002, 'MGUNA0', 'Chaingun'), (2003, 'LAUNA0', 'Rocket launcher'),
    (2004, 'PLASA0', 'Plasma rifle'), (2005, 'CSAWA0', 'Chainsaw'), (2006, 'BFUGA0', 'BFG9000'),
    (2007, 'CLIPA0', 'Clip'), (2048, 'AMMOA0', 'Box of bullets'), (2008, 'SHELA0', '4 shells'),
    (2049, 'SBOXA0', 'Box of shells'), (2010, 'ROCKA0', 'Rocket'), (2046, 'BROKA0', 'Box of rockets'),
    (2047, 'CELLA0', 'Energy cell'), (17, 'CELPA0', 'Energy cell pack'), (8, 'BPAKA0', 'Backpack'),
    (2011, 'STIMA0', 'Stimpack'), (2012, 'MEDIA0', 'Medikit'), (2014, 'BON1A0', 'Health bonus'),
    (2013, 'SOULA0', 'Soulsphere'), (2018, 'ARM1A0', 'Green armor'), (2019, 'ARM2A0', 'Blue armor'),
    (2015, 'BON2A0', 'Armor bonus'), (5, 'BKEYA0', 'Blue keycard'), (6, 'YKEYA0', 'Yellow keycard'),
    (13, 'RKEYA0', 'Red keycard'), (40, 'BSKUA0', 'Blue skull key'), (39, 'YSKUA0', 'Yellow skull key'),
    (38, 'RSKUA0', 'Red skull key'), (2022, 'PINVA0', 'Invulnerability'), (2023, 'PSTRA0', 'Berserk'),
    (2024, 'PINSA0', 'Partial invisibility'), (2025, 'SUITA0', 'Radiation suit'),
    (2026, 'PMAPA0', 'Computer map'), (2045, 'PVISA0', 'Light amplification visor'),
]

# ----------------------------------------------------------------------------
# Quake 3's pk3s
# ----------------------------------------------------------------------------

def tga_to_png(data):
    """uncompressed or RLE true colour / greyscale TGA to PNG; None if not supported"""
    idlen, cmaptype, itype = data[0], data[1], data[2]
    width, height, bpp, desc = struct.unpack_from('<HHBB', data, 12)
    if cmaptype or itype not in (2, 3, 10, 11) or bpp not in (8, 24, 32):
        return None
    pos = 18 + idlen
    px = bpp // 8
    count = width * height
    raw = bytearray(count * px)
    if itype in (2, 3):
        raw[:] = data[pos:pos + count * px]
    else:
        out = 0
        while out < count * px and pos < len(data):
            head = data[pos]
            pos += 1
            n = (head & 0x7f) + 1
            if head & 0x80:
                raw[out:out + n * px] = data[pos:pos + px] * n
                pos += px
            else:
                raw[out:out + n * px] = data[pos:pos + n * px]
                pos += n * px
            out += n * px
    rgba = bytearray(count * 4)
    for i in range(count):
        p = raw[i * px:i * px + px]
        if px == 1:
            rgba[i * 4:i * 4 + 4] = bytes((p[0], p[0], p[0], 255))
        else:
            rgba[i * 4:i * 4 + 4] = bytes((p[2], p[1], p[0], p[3] if px == 4 else 255))
    if not (desc & 0x20):		# bottom-up rows
        stride = width * 4
        rgba = b''.join(rgba[(height - 1 - y) * stride:(height - y) * stride] for y in range(height))
    return png_encode(width, height, bytes(rgba))

class Quake3:
    def __init__(self, baseq3):
        self.files = {}		# lower case path -> (pk3, real name), the last pk3 wins
        self.pk3s = []
        for name in sorted(os.listdir(baseq3), key=str.lower):
            if name.lower().endswith('.pk3'):
                z = zipfile.ZipFile(os.path.join(baseq3, name))
                self.pk3s.append(z)
                for info in z.infolist():
                    self.files[info.filename.lower()] = (z, info.filename)
        self.shaders = self.read_shaders()
        self.images = {}		# image path without extension -> file path
        for path in self.files:
            if path.startswith('textures/') and path.endswith(('.tga', '.jpg')):
                self.images.setdefault(path[:-4], path)

    def read(self, path):
        entry = self.files.get(path.lower())
        return entry[0].read(entry[1]) if entry else None

    def find_image(self, name):
        """an image named with or without its extension"""
        name = name.lower().replace('\\', '/')
        base = name[:-4] if name.endswith(('.tga', '.jpg')) else name
        for ext in ('.tga', '.jpg'):
            if base + ext in self.files:
                return base + ext
        return None

    def read_shaders(self):
        """shader name -> the image that shows it (qer_editorimage, else its first map)"""
        shaders = {}
        for path in self.files:
            if not (path.startswith('scripts/') and path.endswith('.shader')):
                continue
            text = self.read(path).decode('latin-1')
            tokens = []
            for line in text.splitlines():
                line = line.split('//')[0]
                tokens.extend(line.replace('{', ' { ').replace('}', ' } ').split())
            i, depth, current, images = 0, 0, None, []
            while i < len(tokens):
                t = tokens[i]
                if t == '{':
                    depth += 1
                elif t == '}':
                    depth -= 1
                    if depth == 0 and current:
                        shaders.setdefault(current.lower(), [x.lower() for x in images])
                        current = None
                elif depth == 0:
                    current, images = t, []
                elif t.lower() == 'qer_editorimage' and i + 1 < len(tokens):
                    images.insert(0, tokens[i + 1])
                elif t.lower() in ('map', 'clampmap') and i + 1 < len(tokens) and not tokens[i + 1].startswith('$'):
                    images.append(tokens[i + 1])
                elif t.lower() == 'animmap' and i + 2 < len(tokens):
                    images.append(tokens[i + 2])
                i += 1
        return shaders

    def shader_image(self, name):
        """the picture of a shader or image: its editor image or the first of
        its maps that exists, else an image starting with its name (skies)"""
        name = name.lower()
        for candidate in self.shaders.get(name, []) + [name]:
            path = self.find_image(candidate)
            if path:
                return path
        for base, path in self.images.items():
            if base.startswith(name):
                return path
        return None

    def catalogue(self):
        """what can replace Doom's textures: the shaders under textures/ and the
        plain images no shader covers; and the item models"""
        # shaders without any picture in the paks are not offered
        names = set(n for n in self.shaders if n.startswith('textures/') and self.shader_image(n)) | set(self.images)
        models = sorted(p[:-4] + '.md3' for p in self.files
                        if p.endswith('.md3') and p.startswith(('models/powerups/', 'models/weapons2/', 'models/flags/'))
                        and '_hand' not in p and '_flash' not in p and '_barrel' not in p)
        return [{'name': n} for n in sorted(names)], models

    def image_png(self, name):
        """a shader's or image's picture as PNG or JPEG bytes and its type"""
        path = self.shader_image(name)
        if not path:
            return None, None
        data = self.read(path)
        if path.endswith('.jpg'):
            return data, 'image/jpeg'
        png = tga_to_png(data)
        return (png, 'image/png') if png else (None, None)

# ----------------------------------------------------------------------------
# the web page and its server
# ----------------------------------------------------------------------------

PAGE = r'''<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Quake Visuals Editor</title>
<style>
:root { --bg:#16171a; --panel:#202227; --line:#33363d; --text:#e6e6e6; --muted:#9aa0a6; --accent:#d4482b; --ok:#5fb36b; }
* { box-sizing:border-box; }
body { margin:0; background:var(--bg); color:var(--text); font:14px/1.4 system-ui, sans-serif; height:100vh; display:flex; flex-direction:column; }
header { display:flex; align-items:center; gap:12px; padding:8px 14px; border-bottom:1px solid var(--line); background:var(--panel); }
header h1 { font-size:16px; margin:0; }
header .grow { flex:1; }
button { background:#2c2f36; color:var(--text); border:1px solid var(--line); border-radius:4px; padding:5px 10px; cursor:pointer; font:inherit; }
button:hover { border-color:var(--muted); }
button.primary { background:var(--accent); border-color:var(--accent); color:#fff; }
input[type=text], input[type=number] { background:#111214; color:var(--text); border:1px solid var(--line); border-radius:4px; padding:5px 8px; font:inherit; }
main { flex:1; display:grid; grid-template-columns: 1fr 340px 1fr; min-height:0; }
section { display:flex; flex-direction:column; min-height:0; border-right:1px solid var(--line); }
.bar { display:flex; gap:6px; padding:8px; border-bottom:1px solid var(--line); flex-wrap:wrap; align-items:center; }
.tabs button.on { border-color:var(--accent); color:#fff; }
.grid { flex:1; overflow:auto; padding:8px; display:grid; grid-template-columns:repeat(auto-fill, minmax(112px,1fr)); gap:8px; align-content:start; }
.cell { background:var(--panel); border:1px solid var(--line); border-radius:4px; padding:4px; cursor:pointer; }
.cell.on { border-color:var(--accent); }
.cell .pics { display:flex; gap:2px; height:64px; }
.cell .pics img, .cell .pics .none { flex:1; min-width:0; height:64px; object-fit:contain; image-rendering:pixelated; background:#0c0d0f; }
.cell .none { display:flex; align-items:center; justify-content:center; color:var(--muted); font-size:11px; }
.cell .name { font-size:11px; margin-top:3px; overflow:hidden; text-overflow:ellipsis; white-space:nowrap; }
.cell .to { font-size:10px; color:var(--muted); overflow:hidden; text-overflow:ellipsis; white-space:nowrap; }
.cell .to.wild { font-style:italic; }
#detail { padding:12px; overflow:auto; display:flex; flex-direction:column; gap:10px; }
#detail .big { width:100%; height:200px; object-fit:contain; image-rendering:pixelated; background:#0c0d0f; border:1px solid var(--line); }
#detail .label { color:var(--muted); font-size:12px; }
#detail .row { display:flex; gap:6px; align-items:center; flex-wrap:wrap; }
#detail code { background:#111214; padding:2px 5px; border-radius:3px; word-break:break-all; }
.q3cell .pics { height:80px; }
.q3cell .pics img { height:80px; image-rendering:auto; }
.models { flex:1; overflow:auto; padding:8px; }
.models div { padding:4px 6px; border-radius:3px; cursor:pointer; font-family:monospace; font-size:12px; }
.models div:hover { background:var(--panel); }
#status { color:var(--muted); font-size:12px; }
.hint { color:var(--muted); font-size:12px; }
</style></head><body>
<header><h1>Quake Visuals Editor</h1><span id="status"></span><span class="grow"></span>
<button id="revert">Revert</button><button class="primary" id="save">Save quakevisuals.dat</button></header>
<main>
<section>
  <div class="bar tabs" id="tabs"></div>
  <div class="bar"><input type="text" id="dfilter" placeholder="Filter Doom names" style="flex:1">
    <label class="hint"><input type="checkbox" id="unmapped"> only unchanged</label></div>
  <div class="grid" id="doomgrid"></div>
</section>
<section id="detail"><p class="hint">Pick a Doom texture, flat, sky or item on the left, then a Quake 3 one on the right.</p></section>
<section style="border-right:0">
  <div class="bar"><input type="text" id="qfilter" placeholder="Filter Quake 3 names (e.g. base_wall, gothic, sky)" style="flex:1"></div>
  <div class="grid" id="q3grid"></div>
  <div class="models" id="models" style="display:none"></div>
</section>
</main>
<script>
let S = null;			// /api/state
let lines = [];			// the file: {raw} or {kind, pattern, target, extra, scale, raw}
let tab = 'wall', sel = null, dirty = false;
const KINDS = { wall:'Walls', flat:'Flats', sky:'Skies', item:'Items' };

function parse(text) {
  return text.split(/\r?\n/).map(raw => {
    const t = raw.split('#')[0].trim().split(/\s+/);
    if (!t[0] || !['wall','flat','sky','keep','item'].includes(t[0].toLowerCase())) return { raw };
    // src: the line as written, kept until it is edited
    const kind = t[0].toLowerCase();
    if (kind === 'keep') return { kind, pattern: t[1] || '', src: raw };
    if (kind === 'item') return { kind, pattern: t[1] || '', target: t[2] || '', extra: t[3] && t[3] !== '-' ? t[3] : '', scale: t[4] || '', src: raw };
    return { kind, pattern: t[1] || '', target: t[2] || '', scale: t[3] || '', src: raw };
  });
}
function lineText(l) {
  if (l.raw !== undefined) return l.raw;
  if (l.src !== undefined) return l.src;
  if (l.kind === 'keep') return 'keep ' + l.pattern;
  if (l.kind === 'item') return ['item', l.pattern, l.target, (l.extra || (l.scale ? '-' : '')), l.scale].filter(x => x !== '').join(' ');
  return [l.kind, l.pattern, l.target, l.scale].filter(x => x !== '').join(' ');
}
function match(p, n) {
  const re = new RegExp('^' + p.split('*').map(s => s.replace(/[.+?^${}()|[\]\\]/g, '\\$&')).join('.*') + '$', 'i');
  return re.test(n);
}
// the line that decides a Doom name (first match wins; keep counts for walls, flats and skies)
function lookup(kind, name) {
  for (const l of lines) {
    if (l.raw !== undefined) continue;
    if (kind === 'item') { if (l.kind === 'item' && String(l.pattern) === String(name)) return l; continue; }
    if (l.kind !== kind && l.kind !== 'keep') continue;
    if (match(l.pattern, name)) return l;
  }
  return null;
}
// a line for this exact name, put in the edited block at the top
function exactLine(kind, name) {
  let l = lines.find(l => l.raw === undefined && (l.kind === kind || (kind !== 'item' && l.kind === 'keep')) && l.pattern.toUpperCase() === String(name).toUpperCase());
  if (l) return l;
  let at = lines.findIndex(l => l.raw === '# --- edited with qvedit --------------------------------------------------');
  if (at < 0) {
    at = lines.findIndex(l => l.raw === undefined || (l.raw.trim() !== '' && !l.raw.startsWith('#')));
    if (at < 0) at = lines.length;
    lines.splice(at, 0, { raw: '# --- edited with qvedit --------------------------------------------------' }, { raw: '' });
  }
  l = { kind, pattern: String(name), target: '', scale: '' };
  lines.splice(at + 1, 0, l);
  return l;
}
// an edited line is written again from its fields
function edited(l) { delete l.src; changed(); }
function changed() { dirty = true; status('Not saved'); renderDoom(); renderDetail(); }
function status(s) { document.getElementById('status').textContent = s; }
function img(src) { const i = document.createElement('img'); i.loading = 'lazy'; i.src = src; i.onerror = () => { i.replaceWith(none('?')); }; return i; }
function none(t) { const d = document.createElement('div'); d.className = 'none'; d.textContent = t; return d; }
function doomImg(kind, name) {
  if (kind === 'item') return '/doom/sprite/' + S.items.find(i => i.ednum == name).sprite;
  return '/doom/' + (kind === 'sky' ? 'wall' : kind) + '/' + name;
}
function q3Img(name) { return '/q3/' + encodeURIComponent(name); }

function renderTabs() {
  const el = document.getElementById('tabs'); el.innerHTML = '';
  for (const k in KINDS) {
    const b = document.createElement('button'); b.textContent = KINDS[k]; b.className = k === tab ? 'on' : '';
    b.onclick = () => { tab = k; sel = null; renderTabs(); renderDoom(); renderDetail(); renderQ3(); };
    el.appendChild(b);
  }
}
function doomNames() {
  if (tab === 'wall') return S.walls;
  if (tab === 'flat') return S.flats;
  if (tab === 'sky') return S.skies;
  return S.items.map(i => String(i.ednum));
}
function renderDoom() {
  const grid = document.getElementById('doomgrid'), f = document.getElementById('dfilter').value.toUpperCase();
  const only = document.getElementById('unmapped').checked;
  grid.innerHTML = '';
  for (const name of doomNames()) {
    const item = tab === 'item' ? S.items.find(i => i.ednum == name) : null;
    const label = item ? item.name : name;
    if (f && !label.toUpperCase().includes(f) && !name.includes(f)) continue;
    const l = lookup(tab, name);
    const mapped = l && l.kind !== 'keep';
    if (only && mapped) continue;
    const c = document.createElement('div'); c.className = 'cell' + (sel === name ? ' on' : '');
    const pics = document.createElement('div'); pics.className = 'pics';
    pics.appendChild(img(doomImg(tab, name)));
    pics.appendChild(mapped && tab !== 'item' ? img(q3Img(l.target)) : none(mapped ? 'model' : l ? 'kept' : 'Doom'));
    c.appendChild(pics);
    const n = document.createElement('div'); n.className = 'name'; n.textContent = label; c.appendChild(n);
    const to = document.createElement('div');
    to.className = 'to' + (l && l.pattern.includes('*') ? ' wild' : '');
    to.textContent = !l ? '—' : l.kind === 'keep' ? 'keep (' + l.pattern + ')' : l.target.replace(/^textures\//, '') + (l.pattern.includes('*') ? ' (' + l.pattern + ')' : '');
    c.appendChild(to);
    c.onclick = () => { sel = name; renderDoom(); renderDetail(); };
    grid.appendChild(c);
  }
}
function renderDetail() {
  const d = document.getElementById('detail');
  if (sel === null) { d.innerHTML = '<p class="hint">Pick a Doom texture, flat, sky or item on the left, then a Quake 3 one on the right.</p>'; return; }
  const item = tab === 'item' ? S.items.find(i => i.ednum == sel) : null;
  const l = lookup(tab, sel);
  d.innerHTML = '';
  const add = (html) => { const e = document.createElement('div'); e.innerHTML = html; d.appendChild(e); return e; };
  add('<div class="label">Doom</div><b>' + (item ? item.name + ' (' + sel + ')' : sel) + '</b>');
  const big = img(doomImg(tab, sel)); big.className = 'big'; d.appendChild(big);
  if (tab !== 'item') {
    add('<div class="label">Quake 3</div>');
    if (l && l.kind !== 'keep') { const q = img(q3Img(l.target)); q.className = 'big'; q.style.imageRendering = 'auto'; d.appendChild(q); }
  }
  add(!l ? '<span class="hint">Not listed: the Doom look stays.</span>'
    : l.kind === 'keep' ? 'Kept by <code>keep ' + l.pattern + '</code>'
    : 'By <code>' + lineText(l) + '</code>' + (l.pattern.toUpperCase() !== String(sel).toUpperCase() ? '<br><span class="hint">A wildcard line: picking another texture adds a line for this name alone.</span>' : ''));
  if (l && l.kind !== 'keep' && l.pattern.toUpperCase() === String(sel).toUpperCase()) {
    const row = add('<div class="row"><span class="label">' + (tab === 'item' ? 'Scale' : 'Scale (world units per texel)') + '</span></div>').firstChild;
    const s = document.createElement('input'); s.type = 'number'; s.step = '0.25'; s.min = '0'; s.value = l.scale || ''; s.placeholder = tab === 'item' ? '1' : '0.5'; s.style.width = '80px';
    s.onchange = () => { l.scale = s.value; edited(l); }; row.appendChild(s);
    if (tab === 'item') {
      const row2 = add('<div class="row"><span class="label">Second model</span></div>').firstChild;
      const m2 = document.createElement('input'); m2.type = 'text'; m2.value = l.extra || ''; m2.style.flex = '1';
      m2.onchange = () => { l.extra = m2.value.trim(); edited(l); }; row2.appendChild(m2);
    }
  }
  const row = add('<div class="row"></div>').firstChild;
  if (tab !== 'item') {
    const keep = document.createElement('button'); keep.textContent = 'Keep the Doom look';
    keep.onclick = () => { const e = exactLine(tab, sel); e.kind = 'keep'; delete e.target; edited(e); }; row.appendChild(keep);
  }
  const clear = document.createElement('button'); clear.textContent = 'Remove this name\'s line';
  clear.disabled = !l || l.pattern.toUpperCase() !== String(sel).toUpperCase();
  clear.onclick = () => { lines.splice(lines.indexOf(l), 1); changed(); }; row.appendChild(clear);
  add('<p class="hint">' + (tab === 'item' ? 'Click a model on the right to use it.' : 'Click a Quake 3 texture on the right to use it. In the game, vid_restart reloads the file.') + '</p>');
}
function assign(target) {
  if (sel === null) { status('Pick a Doom ' + KINDS[tab].toLowerCase().replace(/s$/, '') + ' first'); return; }
  const e = exactLine(tab, sel);
  if (e.kind === 'keep') { e.kind = tab; e.scale = ''; }
  e.target = target;
  edited(e);
}
function renderQ3() {
  const grid = document.getElementById('q3grid'), models = document.getElementById('models');
  const f = document.getElementById('qfilter').value.toLowerCase().split(/\s+/).filter(x => x);
  if (tab === 'item') {
    grid.style.display = 'none'; models.style.display = '';
    models.innerHTML = '';
    for (const m of S.models) {
      if (f.length && !f.every(w => m.includes(w))) continue;
      const d = document.createElement('div'); d.textContent = m; d.onclick = () => assign(m); models.appendChild(d);
    }
    return;
  }
  grid.style.display = ''; models.style.display = 'none';
  grid.innerHTML = '';
  let shown = 0;
  for (const t of S.q3) {
    if (tab === 'sky' ? !t.name.includes('sky') : false) continue;
    if (f.length && !f.every(w => t.name.includes(w))) continue;
    if (++shown > 600) { grid.appendChild(none('… filter to see more')); break; }
    const c = document.createElement('div'); c.className = 'cell q3cell';
    const pics = document.createElement('div'); pics.className = 'pics'; pics.appendChild(img(q3Img(t.name))); c.appendChild(pics);
    const n = document.createElement('div'); n.className = 'name'; n.title = t.name; n.textContent = t.name.replace(/^textures\//, ''); c.appendChild(n);
    c.onclick = () => assign(t.name);
    grid.appendChild(c);
  }
}
async function load() {
  S = await (await fetch('/api/state')).json();
  lines = parse(S.text);
  dirty = false; status(S.path);
  renderTabs(); renderDoom(); renderDetail(); renderQ3();
}
document.getElementById('dfilter').oninput = renderDoom;
document.getElementById('unmapped').onchange = renderDoom;
document.getElementById('qfilter').oninput = renderQ3;
document.getElementById('revert').onclick = () => { if (!dirty || confirm('Forget the changes?')) load(); };
document.getElementById('save').onclick = async () => {
  const r = await fetch('/api/save', { method: 'POST', body: lines.map(lineText).join('\n').replace(/\n*$/, '\n') });
  const j = await r.json();
  if (j.ok) { dirty = false; status('Saved to ' + j.paths.join(' and ') + ' — vid_restart in the game'); } else status('Not saved: ' + j.error);
};
window.onbeforeunload = () => dirty ? 'unsaved' : undefined;

// ?selftest: the editing steps on the real file, nothing saved
async function selfTest() {
  const out = [], ok = (what, cond) => out.push((cond ? 'PASS ' : 'FAIL ') + what);
  ok('unchanged file written back as read', lines.map(lineText).join('\n').replace(/\n*$/, '') === S.text.replace(/\r/g, '').replace(/\n*$/, ''));
  tab = 'wall'; sel = 'STARTAN3'; assign('textures/base_wall/metalfloor_wall_10');
  ok('a wall gets its Quake 3 texture', lookup('wall', 'STARTAN3').target === 'textures/base_wall/metalfloor_wall_10');
  sel = 'BRNBIGC'; const before = lookup('wall', 'BRNBIGC').pattern; assign('textures/gothic_block/blocks17');
  ok('a name under a wildcard (' + before + ') gets its own line first', lookup('wall', 'BRNBIGC').pattern === 'BRNBIGC' && lookup('wall', 'BRNBIGL').pattern === before);
  tab = 'flat'; sel = 'FLOOR7_2'; const e = exactLine('flat', 'FLOOR7_2'); e.kind = 'keep'; edited(e);
  ok('keep', lookup('flat', 'FLOOR7_2').kind === 'keep');
  tab = 'item'; sel = '2011'; assign('models/powerups/health/small_cross.md3');
  ok('an item model', lookup('item', '2011').target === 'models/powerups/health/small_cross.md3');
  const text = lines.map(lineText).join('\n');
  ok('the edited block comes before the other lines', text.indexOf('wall STARTAN3') < text.indexOf('keep SW1*'));
  document.body.innerHTML = '<pre id="selftest">' + out.join('\n') + '\n---\n' + text.split('\n').slice(0, 30).join('\n') + '</pre>';
}
load().then(() => { if (location.search.includes('selftest')) selfTest(); });
</script></body></html>'''


class Editor:
    def __init__(self, game):
        self.game = game
        self.wad = Wad(os.path.join(game, 'baseq3', 'DOOM.WAD'))
        self.q3 = Quake3(os.path.join(game, 'baseq3'))
        self.out = [os.path.join(game, 'q3doom', 'quakevisuals.dat')]
        data = os.path.normpath(os.path.join(game, '..', 'data', 'q3doom', 'quakevisuals.dat'))
        if os.path.exists(data):
            self.out.append(data)		# the source tree: a build copies data/ over run/
        self.catalogue, self.models = self.q3.catalogue()
        self.cache = {}

    def text(self):
        for path in self.out:
            if os.path.exists(path):
                with open(path, encoding='utf-8', errors='replace') as f:
                    return f.read()
        return '# Q3Doom - Quake Visuals (r_quakeVisuals 1)\n'

    def state(self):
        skies = [s for s in ('SKY1', 'SKY2', 'SKY3', 'SKY4') if s in self.wad.textures]
        return {
            'walls': sorted(n for n in self.wad.textures if n not in skies),
            'flats': sorted(self.wad.flats),
            'skies': skies,
            'items': [{'ednum': e, 'sprite': s, 'name': n} for e, s, n in DOOM_ITEMS],
            'q3': self.catalogue,
            'models': self.models,
            'text': self.text(),
            'path': ' and '.join(self.out),
        }

    def save(self, text):
        written = []
        for path in self.out:
            os.makedirs(os.path.dirname(path), exist_ok=True)
            with open(path, 'w', encoding='utf-8', newline='\n') as f:
                f.write(text)
            written.append(path)
        return written

    def doom_png(self, kind, name):
        key = ('doom', kind, name)
        if key not in self.cache:
            if kind == 'wall':
                self.cache[key] = self.wad.texture_png(name)
            elif kind == 'flat':
                self.cache[key] = self.wad.flat_png(name)
            else:
                self.cache[key] = self.wad.sprite_png(name)
        return self.cache[key]

    def q3_image(self, name):
        key = ('q3', name)
        if key not in self.cache:
            self.cache[key] = self.q3.image_png(name)
        return self.cache[key]


def make_handler(editor):
    class Handler(http.server.BaseHTTPRequestHandler):
        def log_message(self, *args):
            pass

        def send(self, code, body, kind):
            self.send_response(code)
            self.send_header('Content-Type', kind)
            self.send_header('Content-Length', str(len(body)))
            self.send_header('Cache-Control', 'no-store' if kind.startswith(('text/html', 'application/json')) else 'max-age=3600')
            self.end_headers()
            self.wfile.write(body)

        def do_GET(self):
            path = urllib.parse.unquote(urllib.parse.urlparse(self.path).path)
            if path == '/':
                return self.send(200, PAGE.encode('utf-8'), 'text/html; charset=utf-8')
            if path == '/api/state':
                return self.send(200, json.dumps(editor.state()).encode('utf-8'), 'application/json')
            parts = path.strip('/').split('/', 2)
            if len(parts) == 3 and parts[0] == 'doom' and parts[1] in ('wall', 'flat', 'sprite'):
                png = editor.doom_png(parts[1], parts[2])
                if png:
                    return self.send(200, png, 'image/png')
            if len(parts) >= 2 and parts[0] == 'q3':
                data, kind = editor.q3_image(path[len('/q3/'):])
                if data:
                    return self.send(200, data, kind)
            self.send(404, b'not found', 'text/plain')

        def do_POST(self):
            if self.path != '/api/save':
                return self.send(404, b'not found', 'text/plain')
            length = int(self.headers.get('Content-Length', '0'))
            text = self.rfile.read(length).decode('utf-8')
            try:
                paths = editor.save(text)
                body = {'ok': True, 'paths': paths}
            except OSError as e:
                body = {'ok': False, 'error': str(e)}
            self.send(200, json.dumps(body).encode('utf-8'), 'application/json')
    return Handler


def find_game(arg):
    candidates = [arg] if arg else [os.path.join(HERE, '..', '..', 'run'), os.getcwd(), os.path.dirname(sys.executable)]
    for c in candidates:
        if c and os.path.exists(os.path.join(c, 'baseq3', 'DOOM.WAD')):
            return os.path.normpath(c)
    sys.exit('qvedit: no game folder with baseq3/DOOM.WAD found%s; use --game <folder>' % (' at ' + arg if arg else ''))


def main():
    parser = argparse.ArgumentParser(description='Q3Doom Quake Visuals editor')
    parser.add_argument('--game', help='the game folder (holding baseq3/ and q3doom/)')
    parser.add_argument('--port', type=int, default=8765)
    parser.add_argument('--no-browser', action='store_true')
    args = parser.parse_args()

    game = find_game(args.game)
    print('qvedit: reading the Doom and Quake 3 data in', game)
    editor = Editor(game)
    print('qvedit: %d Doom textures, %d flats; %d Quake 3 textures and shaders, %d models'
          % (len(editor.wad.textures), len(editor.wad.flats), len(editor.catalogue), len(editor.models)))
    server = http.server.ThreadingHTTPServer(('127.0.0.1', args.port), make_handler(editor))
    url = 'http://127.0.0.1:%d/' % args.port
    print('qvedit: open', url, '(Ctrl+C to stop)')
    if not args.no_browser:
        threading.Timer(0.5, webbrowser.open, (url,)).start()
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == '__main__':
    main()
