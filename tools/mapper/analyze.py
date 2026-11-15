"""Reads the dumps written by a mapper run and reports the patch byte of each parameter.
usage: analyze.py params.tsv dumps_dir"""
import os, sys

tsv, d = sys.argv[1], sys.argv[2]
def data(name):
    p = os.path.join(d, name + '.syx')
    return open(p, 'rb').read()[16:526] if os.path.exists(p) else None

found, unclear, missing = [], [], []
for row in open(tsv, encoding='utf-8'):
    pid, addr, num, num2, lo, hi, de, off, wide = row.rstrip('\n').split('\t')
    if addr == 'patch':
        continue
    A, B = data(pid + '__a'), data(pid + '__b')
    if A is None or B is None:
        missing.append(pid); continue
    diff = [i for i in range(510) if A[i] != B[i]]
    lo, hi, off, wide = int(lo), int(hi), int(off), wide == '1'
    b = hi if (hi - lo) % 2 == 1 or hi - lo < 2 else hi - 1
    guess = None
    if wide and len(diff) in (1, 2):
        o = diff[0]
        if A[o] == lo >> 1 and B[o] == b >> 1:
            guess = o
    elif len(diff) == 1:
        guess = diff[0]
    note = ''
    if guess is not None and not wide:
        note = 'values %d->%d' % (A[guess], B[guess])
    if guess is None:
        unclear.append('%-22s diff=%s' % (pid, ['%d:%d->%d' % (i, A[i], B[i]) for i in diff[:6]]))
    else:
        check = '' if off < 0 else (' (table %d %s)' % (off, 'OK' if off == guess else 'MISMATCH'))
        found.append((pid, guess, note + check))

for pid, g, note in found:
    print('%-22s %4d  %s' % (pid, g, note))
print('\nunclear (%d):' % len(unclear))
print('\n'.join(unclear))
print('\nno dumps:', ' '.join(missing))
