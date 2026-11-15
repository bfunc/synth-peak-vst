"""Writes a probe plan that finds each parameter's byte in the patch.
For every live parameter: send value A, dump the edit buffer; send value B, dump again. The byte(s) that differ
between the two dumps hold the parameter. The last line writes the original patch back into the edit buffer.
usage: make_plan.py params.tsv channel base.syx out_plan.txt [id-prefix-filter]"""
import sys

tsv, channel, base, out = sys.argv[1], int(sys.argv[2]), sys.argv[3], sys.argv[4]
only = sys.argv[5] if len(sys.argv) > 5 else ''
st = 0xB0 | (channel - 1)

def msgs(addr, num, num2, v):
    if addr == 'cc':
        return [(st, num, v)]
    if addr == 'pair':
        return [(st, num, v >> 1), (st, num2, (v & 1) << 6)]
    return [(st, 99, num), (st, 98, num2), (st, 6, v)]

lines = ['base\t']
for row in open(tsv, encoding='utf-8'):
    pid, addr, num, num2, lo, hi, de, off, wide = row.rstrip('\n').split('\t')
    if addr == 'patch' or not pid.startswith(only):
        continue
    num, num2, lo, hi = int(num), int(num2), int(lo), int(hi)
    a = lo
    b = hi if (hi - lo) % 2 == 1 or hi - lo < 2 else hi - 1   # odd difference: both bytes of a 0-255 pair change
    if pid.startswith('anim'):
        a, b = 0, 127
    for tag, v in (('a', a), ('b', b)):
        lines.append('%s__%s\t%s' % (pid, tag, ' '.join('%02X %02X %02X' % m for m in msgs(addr, num, num2, v))))
restore = open(base, 'rb').read()
lines.append('!restore\t' + ' '.join('%02X' % x for x in restore))
lines.append('restored\t')
open(out, 'w').write('\n'.join(lines) + '\n')
print(len(lines), 'lines')
