import re, sys, os
ROOT = os.path.expanduser('~/Coding/MapleV83/server/wz/Map.wz/Map')
TOWNS = {
    100000000: 'Henesys', 101000000: 'Ellinia', 102000000: 'Perion', 103000000: 'Kerning City',
    104000000: 'Lith Harbor', 105040300: 'Sleepywood', 120000000: 'Nautilus Harbor',
    130000000: 'Ereve', 140000000: 'Rien', 200000000: 'Orbis', 211000000: 'El Nath',
    220000000: 'Ludibrium', 221000000: 'Omega Sector', 222000000: 'Korean Folk Town',
    230000000: 'Aquarium', 240000000: 'Leafre', 250000000: 'Mu Lung', 251000000: 'Herb Town',
    260000000: 'Ariant', 261000000: 'Magatia', 600000000: 'New Leaf City', 680000000: 'Amoria',
    540000000: 'Singapore', 800000000: 'Mushroom Shrine', 801000000: 'Showa Town',
}
NPC = 9010000

def path(mid):
    s = '%09d' % mid
    return os.path.join(ROOT, 'Map%s' % s[0], s + '.img.xml')

def block(xml, name):
    i = xml.find('<imgdir name="%s">' % name)
    if i < 0:
        return ''
    depth, j = 0, i
    for m in re.finditer(r'<imgdir[^>]*?(/?)>|</imgdir>', xml[i:]):
        if m.group(0).startswith('</'):
            depth -= 1
        elif m.group(1) != '/':
            depth += 1
        if depth == 0:
            return xml[i:i + m.end()]
    return xml[i:]

def entries(blk):
    # leaf imgdirs: name -> {int fields, string fields}
    out = []
    for m in re.finditer(r'<imgdir name="([^"]+)">((?:(?!<imgdir).)*?)</imgdir>', blk, re.S):
        d = {k: v for k, v in re.findall(r'<(?:int|string|short|float) name="([^"]+)" value="([^"]*)"', m.group(2))}
        d['_name'] = m.group(1)
        out.append(d)
    return out

rows, report = [], []
for mid, name in TOWNS.items():
    p = path(mid)
    if not os.path.exists(p):
        report.append('%-17s %d: no map data' % (name, mid)); continue
    xml = open(p).read()
    portals = entries(block(xml, 'portal'))
    sp = next((q for q in portals if q.get('pn') == 'sp'), None) or next((q for q in portals if q.get('pt') == '0'), None)
    if not sp:
        report.append('%-17s %d: no spawn' % (name, mid)); continue
    sx, sy = int(sp['x']), int(sp['y'])
    life = [e for e in entries(block(xml, 'life')) if e.get('type') == 'n']
    admins = [e for e in life if e.get('id') == str(NPC)]
    near = [a for a in admins if abs(int(a['x']) - sx) <= 450 and abs(int(a.get('cy', a['y'])) - sy) <= 120]
    if near:
        report.append('%-17s %d: already near spawn (x=%s, spawn %d)' % (name, mid, near[0]['x'], sx)); continue
    fhs = [e for e in entries(block(xml, 'foothold')) if 'x1' in e]
    def ground(x):
        best = None
        for f in fhs:
            x1, y1, x2, y2 = int(f['x1']), int(f['y1']), int(f['x2']), int(f['y2'])
            if x1 == x2 or not (min(x1, x2) <= x <= max(x1, x2)):
                continue
            y = y1 + (y2 - y1) * (x - x1) / (x2 - x1)
            if y >= sy - 25 and (best is None or y < best[0]):
                best = (y, int(f['_name']))
        return best
    placed = None
    npc_xs = [int(e['x']) for e in life]
    land = ground(sx)   # spawns can hang in the air; players drop onto the first ground below
    if not land:
        report.append('%-17s %d: no ground under spawn %d,%d' % (name, mid, sx, sy)); continue
    for gap in (80, 50):   # keep name tags apart; crowded spawns accept a tighter gap
        for dx in (90, -90, 140, -140, 190, -190, 240, -240, 60, -60):
            x = sx + dx
            g = ground(x)
            if not g or abs(g[0] - land[0]) > 40:
                continue
            if any(abs(x - nx) < gap for nx in npc_xs):
                continue
            placed = (x, int(g[0]) - 1, 0); break
        if placed:
            break
    if not placed:
        report.append('%-17s %d: no ground near spawn %d,%d' % (name, mid, sx, sy)); continue
    x, y, fh = placed
    rows.append((mid, name, x, y, fh))
    report.append('%-17s %d: place x=%d y=%d fh=%d (spawn %d,%d)%s' % (name, mid, x, y, fh, sx, sy,
                  '  [far admin x=%s]' % admins[0]['x'] if admins else ''))

print('\n'.join(report))
with open('/tmp/mv83/admin_plife.sql', 'w') as o:
    o.write('-- Maple Administrator (9010000) beside the spawn in every major town (world 0).\n')
    for mid, name, x, y, fh in rows:
        o.write("INSERT INTO plife (world, map, life, type, cy, f, fh, rx0, rx1, x, y, hide, mobtime, team)\n"
                "SELECT 0, %d, 9010000, 'n', %d, 1, %d, %d, %d, %d, %d, 0, 0, 0 FROM DUAL\n"
                "WHERE NOT EXISTS (SELECT 1 FROM plife WHERE map = %d AND life = 9010000); -- %s\n"
                % (mid, y, fh, x - 50, x + 50, x, y, mid, name))
