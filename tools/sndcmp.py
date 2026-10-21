# The OpenAL subset's D3AL_DUMP output compared: host against A1200 (PLAN.md section 14).
#   python3 tools/sndcmp.py HOST.raw HOST.txt AMIGA.raw AMIGA.txt
import sys, array, math, re
host_raw, host_txt, ami_raw, ami_txt = sys.argv[1:5]

def bufs(p):
    out = {}
    for line in open(p, encoding='latin-1'):
        m = re.match(r'buffer (\d+): (\d) ch, (\d+) Hz, (\d+) frames, peak (\d+), mean (\d+), first (.*)', line)
        if m:
            out.setdefault(int(m.group(1)), []).append((int(m.group(2)), int(m.group(3)), int(m.group(4)), int(m.group(5)), int(m.group(6)), m.group(7)))
    return out

h, a = bufs(host_txt), bufs(ami_txt)
same = diff = 0
shown = 0
for k in sorted(h):
    if k not in a:
        continue
    hv, av = h[k][0], a[k][0]
    if hv[:3] == av[:3] and abs(hv[3] - av[3]) <= 2 and abs(hv[4] - av[4]) <= 2:
        same += 1
    else:
        diff += 1
        if shown < 12:
            print('buffer %d differs: host %s | amiga %s' % (k, hv, av))
            shown += 1
print('first loads of each buffer: %d alike, %d different (of %d host, %d amiga)' % (same, diff, len(h), len(a)))

def load(p, swap):
    x = array.array('h')
    x.frombytes(open(p, 'rb').read())
    if swap:
        x.byteswap()
    return x

def env(x, rate=22050):
    r = []
    for s in range(len(x) // 2 // rate):
        seg = x[s * rate * 2:(s + 1) * rate * 2]
        r.append(int(math.sqrt(sum(v * v for v in seg) / len(seg))))
    return r

hx = load(host_raw, False)                 # host: little-endian file
ax = load(ami_raw, sys.byteorder == 'little')  # amiga: big-endian file
print('host  rms/s:', env(hx))
print('amiga rms/s:', env(ax))
ay = array.array('h', ax)
ay.byteswap()
print('amiga, bytes swapped, rms/s:', env(ay))
print('amiga peak %d, clipped %d of %d' % (max(abs(v) for v in ax), sum(1 for v in ax if abs(v) >= 32767), len(ax)))
