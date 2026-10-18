#!/usr/bin/env python3
# TechArena solver (pure Python, stdlib only): simulated annealing with incremental
# convex-hull conflict tracking. Reads instance.json, writes solution.json.
import json, math, os, random, sys, time

T_START = time.perf_counter()
INF = 1e300
rnd = random.Random(12345)
random_ = rnd.random
randrange = rnd.randrange


def now():
    return time.perf_counter() - T_START


# ---------------------------------------------------------------- instance
def read_instance(path):
    global nS, nO, N, Amax, alphaC, X, Y, rew, ocost, omax, ntask, sk, D, workersSk, capSk, servable, nSkill
    with open(path, encoding='utf-8-sig') as f:
        d = json.load(f)
    sites, offs = d['sites'], d['offices']
    nS, nO = len(sites), len(offs)
    N = nS + nO
    X = [float(s['x']) for s in sites] + [float(o['x']) for o in offs]
    Y = [float(s['y']) for s in sites] + [float(o['y']) for o in offs]
    rew = [float(s['reward']) for s in sites]
    ntask = [len(s['tasks']) for s in sites]
    sk = [sorted(set(int(t) for t in s['tasks'])) for s in sites]
    ocost = [float(o['cost']) for o in offs]
    omax = [int(o['maxSize']) for o in offs]
    workersSk = [sorted(set(int(t) for t in w)) for w in d['workers']]
    mx = 0
    for l in sk:
        for k in l:
            mx = max(mx, k)
    for l in workersSk:
        for k in l:
            mx = max(mx, k)
    nSkill = mx + 1
    capSk = [0] * nSkill
    for l in workersSk:
        for k in l:
            capSk[k] += 1
    D = [[float(v) for v in row] for row in d['distances']]
    Amax = int(d['maxDifference'])
    alphaC = float(d['balanceCoefficient'])
    servable = [all(capSk[k] > 0 for k in sk[s]) for s in range(nS)]


# ---------------------------------------------------------------- grid
def build_grid():
    global GW, GH, gx0, gy0, gcw, gch, gcells, TOL, TOL2
    x0, x1, y0, y1 = min(X), max(X), min(Y), max(Y)
    ext = max(x1 - x0, y1 - y0)
    if ext <= 0:
        ext = 1.0
    TOL = 1e-10 * ext
    TOL2 = TOL * TOL
    g = max(1, int(math.sqrt(N / 2.0)))
    GW = GH = g
    gx0, gy0 = x0, y0
    gcw = (x1 - x0) / GW * (1 + 1e-12) if x1 - x0 > 0 else 1.0
    gch = (y1 - y0) / GH * (1 + 1e-12) if y1 - y0 > 0 else 1.0
    gcells = [[] for _ in range(GW * GH)]
    for i in range(N):
        gcells[gcy(Y[i]) * GW + gcx(X[i])].append(i)


def gcx(x):
    c = int(math.floor((x - gx0) / gcw))
    return 0 if c < 0 else (GW - 1 if c >= GW else c)


def gcy(y):
    c = int(math.floor((y - gy0) / gch))
    return 0 if c < 0 else (GH - 1 if c >= GH else c)


def scan_box(x0, y0, x1, y1):
    ix0, ix1 = gcx(x0 - TOL), gcx(x1 + TOL)
    iy0, iy1 = gcy(y0 - TOL), gcy(y1 + TOL)
    out = []
    for iy in range(iy0, iy1 + 1):
        base = iy * GW
        for ix in range(ix0, ix1 + 1):
            out += gcells[base + ix]
    return out


# ---------------------------------------------------------------- hull
def hull_of(ids):
    ids = sorted(ids, key=lambda a: (X[a], Y[a]))
    u = []
    for a in ids:
        if u and X[u[-1]] == X[a] and Y[u[-1]] == Y[a]:
            continue
        u.append(a)
    if len(u) <= 2:
        return u
    lo = []
    for p in u:
        while len(lo) >= 2:
            a, b = lo[-2], lo[-1]
            if (X[b] - X[a]) * (Y[p] - Y[a]) - (Y[b] - Y[a]) * (X[p] - X[a]) <= 0:
                lo.pop()
            else:
                break
        lo.append(p)
    up = []
    for p in reversed(u):
        while len(up) >= 2:
            a, b = up[-2], up[-1]
            if (X[b] - X[a]) * (Y[p] - Y[a]) - (Y[b] - Y[a]) * (X[p] - X[a]) <= 0:
                up.pop()
            else:
                break
        up.append(p)
    return lo[:-1] + up[:-1]


def in_hull(h, x, y):
    m = len(h)
    if m == 0:
        return False
    if m == 1:
        return abs(X[h[0]] - x) <= TOL and abs(Y[h[0]] - y) <= TOL
    if m == 2:
        ax, ay = X[h[0]], Y[h[0]]
        ex, ey = X[h[1]] - ax, Y[h[1]] - ay
        l2 = ex * ex + ey * ey
        cr = ex * (y - ay) - ey * (x - ax)
        if cr * cr > TOL2 * l2:
            return False
        dt = (x - ax) * ex + (y - ay) * ey
        l = math.sqrt(l2)
        return -TOL * l <= dt <= l2 + TOL * l
    a = h[-1]
    ax, ay = X[a], Y[a]
    for b in h:
        bx, by = X[b], Y[b]
        ex, ey = bx - ax, by - ay
        cr = ex * (y - ay) - ey * (x - ax)
        if cr < 0 and cr * cr > TOL2 * (ex * ex + ey * ey):
            return False
        ax, ay = bx, by
    return True


def grow_hull(h, p, bb):
    m = len(h)
    if m <= 2:
        for v in h:
            bbadd(bb, v)
        bbadd(bb, p)
        return hull_of(h + [p])
    vis = [False] * m
    nv = 0
    xp, yp = X[p], Y[p]
    for i in range(m):
        a = h[i]
        b = h[i + 1] if i + 1 < m else h[0]
        cr = (X[b] - X[a]) * (yp - Y[a]) - (Y[b] - Y[a]) * (xp - X[a])
        if cr < 0:
            vis[i] = True
            nv += 1
    if nv == 0:
        return h
    if nv == m:
        for v in h:
            bbadd(bb, v)
        bbadd(bb, p)
        return hull_of(h + [p])
    i0 = -1
    for i in range(m):
        if vis[i] and not vis[(i + m - 1) % m]:
            i0 = i
            break
    bbadd(bb, p)
    for j in range(nv + 1):
        bbadd(bb, h[(i0 + j) % m])
    t2 = (i0 + nv) % m
    nh = [p]
    for j in range(m - nv + 1):
        nh.append(h[(t2 + j) % m])
    return nh


def bbadd(bb, p):
    x, y = X[p], Y[p]
    if x < bb[0]:
        bb[0] = x
    if x > bb[2]:
        bb[2] = x
    if y < bb[1]:
        bb[1] = y
    if y > bb[3]:
        bb[3] = y


# ---------------------------------------------------------------- state
class ZC:
    __slots__ = ('z', 'hv', 'added', 'removed', 'opens', 'closes', 'changed')

    def __init__(self, z):
        self.z = z
        self.hv = None
        self.added = []
        self.removed = []
        self.opens = False
        self.closes = False
        self.changed = False


class Pend:
    pass


pend = Pend()


def init_state():
    global asg, posIn, cnt, load, ocnt, cover, mem, nsk, hv, covList, inZ, V
    global sumProfit, openCost, imbNow, openList
    asg = [-1] * nS
    posIn = [0] * nS
    cnt = [0] * nO
    load = [0] * nO
    ocnt = [0] * nSkill
    cover = [0] * N
    mem = [[] for _ in range(nO)]
    nsk = [[0] * nSkill for _ in range(nO)]
    hv = [[] for _ in range(nO)]
    covList = [[] for _ in range(nO)]
    inZ = [bytearray(N) for _ in range(nO)]
    V = 0
    sumProfit = openCost = imbNow = 0.0
    openList = []


def cur_obj():
    return sumProfit - openCost - alphaC * imbNow


def energy():
    return -(sumProfit - openCost) + alphaEff * imbNow + lambdaV * V


def recompute_imb():
    global imbNow
    if not openList:
        imbNow = 0.0
        return
    ls = [load[z] for z in openList]
    imbNow = float(max(ls) - min(ls))


def zone_change(z, rem, add):
    zc = ZC(z)
    newcnt = cnt[z] - (1 if rem >= 0 else 0) + (1 if add >= 0 else 0)
    if newcnt == 0:
        zc.closes = True
        zc.removed = list(covList[z])
        zc.hv = []
        zc.changed = True
        return zc
    if cnt[z] == 0:
        h = hull_of([nS + z, add])
        zc.hv = h
        zc.opens = True
        zc.changed = True
        bb = [INF, INF, -INF, -INF]
        for v in h:
            bbadd(bb, v)
        zc.added = [p for p in scan_box(bb[0], bb[1], bb[2], bb[3]) if in_hull(h, X[p], Y[p])]
        return zc
    hz = hv[z]
    nh = hz
    bb = [INF, INF, -INF, -INF]
    if rem >= 0:
        idx = -1
        xr, yr = X[rem], Y[rem]
        for i, v in enumerate(hz):
            if X[v] == xr and Y[v] == yr:
                idx = i
                break
        if idx >= 0:
            m = len(hz)
            if m <= 3:
                for v in hz:
                    bbadd(bb, v)
            else:
                bbadd(bb, hz[(idx + m - 1) % m])
                bbadd(bb, rem)
                bbadd(bb, hz[(idx + 1) % m])
            ids = [q for q in mem[z] if q != rem]
            ids.append(nS + z)
            nh = hull_of(ids)
    if add >= 0:
        if not in_hull(nh, X[add], Y[add]):
            nh = grow_hull(nh, add, bb)
    if bb[0] > bb[2]:
        return zc
    zc.changed = True
    zc.hv = nh
    iz = inZ[z]
    added, removed = zc.added, zc.removed
    for p in scan_box(bb[0], bb[1], bb[2], bb[3]):
        nw = in_hull(nh, X[p], Y[p])
        if nw:
            if not iz[p]:
                added.append(p)
        elif iz[p]:
            removed.append(p)
    return zc


def dv_of(zcs):
    dV = 0
    for zc in zcs:
        for p in zc.removed:
            if cover[p] >= 2:
                dV -= 1
            cover[p] -= 1
        for p in zc.added:
            cover[p] += 1
            if cover[p] >= 2:
                dV += 1
    for zc in reversed(zcs):
        for p in zc.added:
            cover[p] -= 1
        for p in zc.removed:
            cover[p] += 1
    return dV


def commit_zone(zc):
    global V
    if not zc.changed:
        return
    z = zc.z
    iz = inZ[z]
    for p in zc.removed:
        if cover[p] >= 2:
            V -= 1
        cover[p] -= 1
        iz[p] = 0
    for p in zc.added:
        cover[p] += 1
        if cover[p] >= 2:
            V += 1
        iz[p] = 1
    hv[z] = zc.hv
    if zc.closes:
        covList[z] = []
    else:
        if zc.removed:
            covList[z] = [p for p in covList[z] if iz[p]]
        covList[z].extend(zc.added)


def bal_scan(za, dca, dla, zb, dcb, dlb):
    mxC = -1
    mnC = 1 << 60
    mxL = -(1 << 60)
    mnL = 1 << 60
    for z in openList:
        c = cnt[z]
        l = load[z]
        if z == za:
            c += dca
            l += dla
        if z == zb:
            c += dcb
            l += dlb
        if c > 0:
            if c > mxC:
                mxC = c
            if c < mnC:
                mnC = c
            if l > mxL:
                mxL = l
            if l < mnL:
                mnL = l
    if zb >= 0 and cnt[zb] == 0:
        c = dcb
        l = dlb
        if c > mxC:
            mxC = c
        if c < mnC:
            mnC = c
        if l > mxL:
            mxL = l
        if l < mnL:
            mnL = l
    if mxC < 0:
        return 0, 0, 0.0
    return mxC, mnC, float(mxL - mnL)


def eval_move(s, b, theta, check):
    a = asg[s]
    ds = sk[s]
    dProfit = 0.0
    dOpen = 0.0
    if b >= 0:
        dProfit += rew[s] - D[b][s]
        if cnt[b] == 0:
            dOpen += ocost[b]
        if check and cnt[b] + 1 > omax[b]:
            return INF
    if a >= 0:
        dProfit -= rew[s] - D[a][s]
        if cnt[a] == 1:
            dOpen -= ocost[a]
    if check and b >= 0:
        nb = nsk[b]
        for k in ds:
            if nb[k] == 0:
                aFrees = a >= 0 and nsk[a][k] == 1
                if ocnt[k] + 1 - (1 if aFrees else 0) > capSk[k]:
                    return INF
    nt = ntask[s]
    mxC, mnC, imb = bal_scan(a, -1, -nt, b, 1, nt)
    if check and mxC >= 0 and mxC - mnC > Amax:
        return INF
    partial = -(dProfit - dOpen) + alphaEff * (imb - imbNow)
    if check:
        if V == 0:
            if partial > theta:
                return INF
        elif partial - lambdaV * V > theta:
            return INF
    pend.type = 1
    pend.s = s
    pend.a = a
    pend.b = b
    pend.dProfit = dProfit
    pend.dOpen = dOpen
    zcs = []
    pend.za = pend.zb = None
    if a >= 0:
        pend.za = zone_change(a, s, -1)
        zcs.append(pend.za)
    if b >= 0:
        pend.zb = zone_change(b, -1, s)
        zcs.append(pend.zb)
    dV = dv_of(zcs)
    return partial + lambdaV * dV


def eval_swap(s, t, theta):
    a, b = asg[s], asg[t]
    dProfit = (-(rew[s] - D[a][s]) - (rew[t] - D[b][t]) + (rew[s] - D[b][s]) + (rew[t] - D[a][t]))
    ss, st = sk[s], sk[t]
    for k in set(ss) | set(st):
        inS = k in ss
        inT = k in st
        na = nsk[a][k] - inS + inT
        nb = nsk[b][k] - inT + inS
        dd = (na > 0) - (nsk[a][k] > 0) + (nb > 0) - (nsk[b][k] > 0)
        if dd > 0 and ocnt[k] + dd > capSk[k]:
            return INF
    dl = ntask[t] - ntask[s]
    mxC, mnC, imb = bal_scan(a, 0, dl, b, 0, -dl)
    partial = -dProfit + alphaEff * (imb - imbNow)
    if V == 0:
        if partial > theta:
            return INF
    elif partial - lambdaV * V > theta:
        return INF
    pend.type = 2
    pend.s = s
    pend.t = t
    pend.a = a
    pend.b = b
    pend.dProfit = dProfit
    pend.dOpen = 0.0
    pend.za = zone_change(a, s, t)
    pend.zb = zone_change(b, t, s)
    dV = dv_of([pend.za, pend.zb])
    return partial + lambdaV * dV


def mem_remove(z, s):
    p = posIn[s]
    last = mem[z][-1]
    mem[z][p] = last
    posIn[last] = p
    mem[z].pop()


def mem_add(z, s):
    posIn[s] = len(mem[z])
    mem[z].append(s)


def skills_out(z, s):
    nz = nsk[z]
    for k in sk[s]:
        nz[k] -= 1
        if nz[k] == 0:
            ocnt[k] -= 1


def skills_in(z, s):
    nz = nsk[z]
    for k in sk[s]:
        if nz[k] == 0:
            ocnt[k] += 1
        nz[k] += 1


def commit_pending():
    global sumProfit, openCost
    if pend.type == 1:
        s, a, b = pend.s, pend.a, pend.b
        if a >= 0:
            mem_remove(a, s)
            cnt[a] -= 1
            load[a] -= ntask[s]
            skills_out(a, s)
            commit_zone(pend.za)
            if cnt[a] == 0:
                openList.remove(a)
        if b >= 0:
            mem_add(b, s)
            if cnt[b] == 0:
                openList.append(b)
            cnt[b] += 1
            load[b] += ntask[s]
            skills_in(b, s)
            commit_zone(pend.zb)
        asg[s] = b
    else:
        s, t, a, b = pend.s, pend.t, pend.a, pend.b
        mem_remove(a, s)
        mem_remove(b, t)
        mem_add(a, t)
        mem_add(b, s)
        load[a] += ntask[t] - ntask[s]
        load[b] += ntask[s] - ntask[t]
        skills_out(a, s)
        skills_out(b, t)
        skills_in(a, t)
        skills_in(b, s)
        commit_zone(pend.za)
        commit_zone(pend.zb)
        asg[s] = b
        asg[t] = a
    sumProfit += pend.dProfit
    openCost += pend.dOpen
    recompute_imb()


def after_accept():
    global bestObj, bestAsg
    if V == 0:
        o = cur_obj()
        if o > bestObj + 1e-9:
            bestObj = o
            bestAsg = asg[:]


def apply_force(s, b):
    eval_move(s, b, INF, False)
    commit_pending()


def count_balance_ok():
    if not openList:
        return True
    cs = [cnt[z] for z in openList]
    return max(cs) - min(cs) <= Amax


def skill_legal(s, b):
    nb = nsk[b]
    for k in sk[s]:
        if nb[k] == 0 and ocnt[k] + 1 > capSk[k]:
            return False
    return True


# ---------------------------------------------------------------- debug
def debug_check(tag):
    c2 = [0] * nO
    l2 = [0] * nO
    ns2 = [[0] * nSkill for _ in range(nO)]
    prof = 0.0
    for s in range(nS):
        z = asg[s]
        if z >= 0:
            c2[z] += 1
            l2[z] += ntask[s]
            for k in sk[s]:
                ns2[z][k] += 1
            prof += rew[s] - D[z][s]
    ok = True
    if abs(prof - sumProfit) > 1e-6 * (1 + abs(prof)):
        print(tag, 'profit mismatch', prof, sumProfit, file=sys.stderr)
        ok = False
    for z in range(nO):
        if c2[z] != cnt[z] or l2[z] != load[z] or len(mem[z]) != cnt[z] or ns2[z] != nsk[z]:
            print(tag, 'zone mismatch', z, file=sys.stderr)
            ok = False
    if sorted(openList) != [z for z in range(nO) if c2[z] > 0]:
        print(tag, 'openList mismatch', file=sys.stderr)
        ok = False
    cov2 = [0] * N
    for z in range(nO):
        if c2[z] == 0:
            continue
        ids = [s for s in range(nS) if asg[s] == z] + [nS + z]
        h = hull_of(ids)
        for p in range(N):
            if in_hull(h, X[p], Y[p]):
                cov2[p] += 1
                if not inZ[z][p]:
                    print(tag, 'inZ missing', z, p, file=sys.stderr)
                    ok = False
            elif inZ[z][p]:
                print(tag, 'inZ extra', z, p, file=sys.stderr)
                ok = False
    v2 = sum(c - 1 for c in cov2 if c >= 2)
    if v2 != V or cover != cov2 and sum(c - 1 for c in cover if c >= 2) != V:
        print(tag, 'V mismatch', v2, V, file=sys.stderr)
        ok = False
    if not ok:
        print('DEBUG CHECK FAILED', tag, file=sys.stderr)
        sys.exit(2)


# ---------------------------------------------------------------- setup helpers
def build_neighbors(KN=10):
    global knn
    knn = [[] for _ in range(nS)]
    sv = servList
    m = len(sv)
    if m == 0:
        return
    x0 = min(X[s] for s in sv)
    x1 = max(X[s] for s in sv)
    y0 = min(Y[s] for s in sv)
    y1 = max(Y[s] for s in sv)
    g = max(1, int(math.sqrt(m / 3.0)))
    cw = (x1 - x0) / g * (1 + 1e-12) if x1 - x0 > 0 else 1.0
    ch = (y1 - y0) / g * (1 + 1e-12) if y1 - y0 > 0 else 1.0
    cell = [[] for _ in range(g * g)]

    def cxf(x):
        return min(max(int((x - x0) / cw), 0), g - 1)

    def cyf(y):
        return min(max(int((y - y0) / ch), 0), g - 1)

    for s in sv:
        cell[cyf(Y[s]) * g + cxf(X[s])].append(s)
    for s in sv:
        cx0, cy0 = cxf(X[s]), cyf(Y[s])
        cand = []
        for r in range(1, g + 2):
            cand = []
            for iy in range(max(0, cy0 - r), min(g - 1, cy0 + r) + 1):
                for ix in range(max(0, cx0 - r), min(g - 1, cx0 + r) + 1):
                    for q in cell[iy * g + ix]:
                        if q != s:
                            dx = X[q] - X[s]
                            dy = Y[q] - Y[s]
                            cand.append((dx * dx + dy * dy, q))
            if len(cand) >= KN + 4 or r > g:
                break
        cand.sort()
        knn[s] = [q for _, q in cand[:KN]]


def greedy_init():
    global bestObj, bestAsg
    best = 0.0
    bo = -1
    bsel = []
    for o in range(nO):
        v = []
        for s in servList:
            pr = rew[s] - D[o][s]
            if pr > 0:
                v.append((-pr, s))
        v.sort()
        take = min(len(v), omax[o])
        tot = -ocost[o] + sum(-v[i][0] for i in range(take))
        if tot > best:
            best = tot
            bo = o
            bsel = [v[i][1] for i in range(take)]
    if bo >= 0:
        for s in bsel:
            apply_force(s, bo)
    bestObj = 0.0
    bestAsg = [-1] * nS
    if V == 0 and cur_obj() > bestObj:
        bestObj = cur_obj()
        bestAsg = asg[:]


# ---------------------------------------------------------------- macros
hist = []


def undo_hist():
    global hist
    for s, old in reversed(hist):
        apply_force(s, old)
    hist = []


def accept_macro(E0, T):
    if not count_balance_ok():
        return False
    dE = energy() - E0
    return dE <= -T * math.log(random_() + 1e-300)


def macro_open(T):
    global hist
    closed = [z for z in range(nO) if cnt[z] == 0 and omax[z] > 0]
    if not closed:
        return
    o = closed[randrange(len(closed))]
    k = len(openList)
    tot = sum(cnt[z] for z in openList)
    frac = 0.4 + 1.2 * random_()
    m = int(frac * len(servList) * 0.5) if k == 0 else int(frac * tot / (k + 1))
    m = max(1, min(m, omax[o]))
    E0 = energy()
    hist = []
    mode = random_()
    order = ordByEuc[o]
    if mode < 0.2:
        order = ordByDist[o]
    elif mode < 0.45:
        sd = servList[randrange(len(servList))]
        order = sorted(servList, key=lambda a: (X[a] - X[sd]) ** 2 + (Y[a] - Y[sd]) ** 2)
    taken = 0
    for s in order:
        if taken >= m:
            break
        if asg[s] == o:
            continue
        if rew[s] - D[o][s] <= 0:
            continue
        if not skill_legal(s, o):
            continue
        hist.append((s, asg[s]))
        apply_force(s, o)
        taken += 1
    if taken == 0:
        return
    if cnt[o] <= omax[o] and accept_macro(E0, T):
        hist = []
        after_accept()
        return
    undo_hist()


def macro_close(T):
    global hist
    if not openList:
        return
    a = openList[randrange(len(openList))]
    E0 = energy()
    hist = []
    for s in list(mem[a]):
        bestZ = -1
        bestP = 0.0
        cands = set(asg[q] for q in knn[s])
        cands.update(openList)
        for z in cands:
            if z < 0 or z == a or cnt[z] == 0 or cnt[z] + 1 > omax[z]:
                continue
            if not skill_legal(s, z):
                continue
            pr = rew[s] - D[z][s]
            if pr > bestP:
                bestP = pr
                bestZ = z
        hist.append((s, a))
        apply_force(s, bestZ)
    if accept_macro(E0, T):
        hist = []
        after_accept()
        return
    undo_hist()


def macro_split(T):
    global hist
    closed = [z for z in range(nO) if cnt[z] == 0 and omax[z] > 0]
    if not openList or not closed:
        return
    a = openList[randrange(len(openList))]
    o = closed[randrange(len(closed))]
    if cnt[a] < 2:
        return
    for _ in range(16):
        ang = random_() * 2 * math.pi
        nx, ny = math.cos(ang), math.sin(ang)
        pa = nx * X[nS + a] + ny * Y[nS + a]
        po = nx * X[nS + o] + ny * Y[nS + o]
        if po <= pa:
            continue
        v = sorted(((nx * X[s] + ny * Y[s], s) for s in mem[a]), reverse=True)
        ca = len(v)
        m = int((0.15 + 0.7 * random_()) * ca)
        m = max(1, min(m, min(omax[o], ca - 1)))
        th = (v[m - 1][0] + v[m][0]) / 2
        if not (pa < th < po):
            continue
        E0 = energy()
        hist = []
        for i in range(m):
            s = v[i][1]
            if not skill_legal(s, o):
                continue
            hist.append((s, a))
            apply_force(s, o)
        if not hist:
            return
        if accept_macro(E0, T):
            hist = []
            after_accept()
            return
        undo_hist()
        return


def macro_shift(T):
    global hist
    if len(openList) < 2:
        return
    a = openList[randrange(len(openList))]
    b = openList[randrange(len(openList))]
    if a == b:
        return
    dx = X[nS + b] - X[nS + a]
    dy = Y[nS + b] - Y[nS + a]
    ang = math.atan2(dy, dx) + (random_() - 0.5) * math.pi * 0.8
    nx, ny = math.cos(ang), math.sin(ang)
    v = sorted(((nx * X[s] + ny * Y[s], s) for s in mem[a] + mem[b]))
    n = len(v)
    if n < 2:
        return
    j = 1 + randrange(n - 1)
    if j > omax[a] or n - j > omax[b]:
        return
    mv = []
    for i in range(n):
        s = v[i][1]
        want = a if i < j else b
        if asg[s] != want:
            mv.append((s, want))
    if not mv:
        return
    E0 = energy()
    hist = []
    for s, w in mv:
        if not skill_legal(s, w):
            continue
        hist.append((s, asg[s]))
        apply_force(s, w)
    if not hist:
        return
    if cnt[a] <= omax[a] and cnt[b] <= omax[b] and accept_macro(E0, T):
        hist = []
        after_accept()
        return
    undo_hist()


# ---------------------------------------------------------------- final
def full_conflicts(asn, want_zones=False):
    ids = [[] for _ in range(nO)]
    for s in range(nS):
        if asn[s] >= 0:
            ids[asn[s]].append(s)
    cov = [0] * N
    zof = [[] for _ in range(N)] if want_zones else None
    for z in range(nO):
        if not ids[z]:
            continue
        h = hull_of(ids[z] + [nS + z])
        bb = [INF, INF, -INF, -INF]
        for q in h:
            bbadd(bb, q)
        for p in scan_box(bb[0], bb[1], bb[2], bb[3]):
            if in_hull(h, X[p], Y[p]):
                cov[p] += 1
                if want_zones:
                    zof[p].append(z)
    cz = []
    c = 0
    for p in range(N):
        if cov[p] >= 2:
            c += 1
            if want_zones:
                cz.extend(zof[p])
    return c, cz


def assign_workers(need):
    nW = len(workersSk)
    n = len(need)
    order = sorted(range(n), key=lambda i: -len(need[i]))
    r2 = random.Random(7)
    for attempt in range(500):
        if attempt:
            r2.shuffle(order)
        used = [False] * nW
        out = [[] for _ in range(n)]
        ok = True
        for oi in order:
            unc = list(need[oi])
            while unc:
                k = unc[-1]
                bw, bs = -1, -1
                for w in range(nW):
                    if not used[w] and k in workersSk[w]:
                        c = sum(1 for q in unc if q in workersSk[w])
                        sc = c * 1000 - len(workersSk[w]) + (r2.randrange(3) if attempt else 0)
                        if sc > bs:
                            bs, bw = sc, w
                if bw < 0:
                    ok = False
                    break
                used[bw] = True
                out[oi].append(bw)
                unc = [q for q in unc if q not in workersSk[bw]]
            if not ok:
                break
        if ok:
            return out
    return None



# ---------------------------------------------------------------- phase 1: power-diagram partition search

def balance_zones(K, sel, rest):
    k = len(K)
    loads = [sum(ntask[s] for s in sel[i]) for i in range(k)]
    prof = lambda i, s: rew[s] - D[K[i]][s]
    bestv = None
    bestsel = sel
    for T in sorted(set(l for l in loads if l > 0)):
        ns = []
        tot = 0.0
        lds = []
        for i in range(k):
            if loads[i] < T or not sel[i]:
                ns.append([])
                continue
            cur = list(sel[i])
            ld = loads[i]
            removed = []
            if ld > T:
                order = sorted((s for s in cur if ntask[s] > 0), key=lambda s: prof(i, s) / ntask[s])
                rm = set()
                for s in order:
                    if ld <= T:
                        break
                    rm.add(s)
                    ld -= ntask[s]
                if rm:
                    cur = [s for s in cur if s not in rm]
                    removed = [s for s in sel[i] if s in rm]
            pool = sorted(rest[i] + removed, key=lambda s: -prof(i, s))
            cap = omax[K[i]]
            for s in pool:
                if len(cur) >= cap:
                    break
                if ld + ntask[s] <= T:
                    cur.append(s)
                    ld += ntask[s]
            ns.append(cur)
            lds.append(ld)
            tot += sum(prof(i, s) for s in cur) - ocost[K[i]]
        if not lds:
            continue
        v = tot - alphaC * (max(lds) - min(lds))
        if bestv is None or v > bestv:
            bestv = v
            bestsel = ns
    return bestsel

def eval_partition(K, w, cx=None, cy=None):
    k = len(K)
    ox = cx if cx is not None else [X[nS + o] for o in K]
    oy = cy if cy is not None else [Y[nS + o] for o in K]
    for i in range(k):  # each office must lie in its own (convex, disjoint) cell -> no hull conflicts
        px, py = X[nS + K[i]], Y[nS + K[i]]
        bv = INF
        bi = -1
        for j in range(k):
            v = (px - ox[j]) ** 2 + (py - oy[j]) ** 2 - w[j]
            if v < bv:
                bv, bi = v, j
        if bi != i:
            return None
    cand = [[] for _ in range(k)]
    rk = range(k)
    for s in servList:
        xs, ys = X[s], Y[s]
        bv = INF
        bi = 0
        for j in rk:
            v = (xs - ox[j]) ** 2 + (ys - oy[j]) ** 2 - w[j]
            if v < bv:
                bv, bi = v, j
        pr = rew[s] - D[K[bi]][s]
        if pr > 0:
            cand[bi].append((-pr, s))
    keep = [[True] * nSkill for _ in rk]
    for kk in range(nSkill):
        tot = []
        for i in rk:
            t = 0.0
            need = False
            for npr, s in cand[i]:
                if kk in sk[s]:
                    t -= npr
                    need = True
            if need:
                tot.append((-t, i))
        if len(tot) > capSk[kk]:
            tot.sort()
            for _, i in tot[capSk[kk]:]:
                keep[i][kk] = False
    sel = []
    rest = []
    for i in rk:
        c = sorted(cand[i])
        lst = []
        rst = []
        for npr, s in c:
            if not all(keep[i][q] for q in sk[s]):
                continue
            if len(lst) >= omax[K[i]]:
                rst.append(s)
            else:
                lst.append(s)
        sel.append(lst)
        rest.append(rst)
    if k >= 2 and alphaC > 0:
        sel = balance_zones(K, sel, rest)
    while True:
        cs = [len(l) for l in sel if l]
        if not cs or max(cs) - min(cs) <= Amax:
            break
        imx = max(range(k), key=lambda i: len(sel[i]))
        sel[imx].pop()
    profit = 0.0
    oc = 0.0
    loads = []
    asn = [-1] * nS
    for i in rk:
        if not sel[i]:
            continue
        oc += ocost[K[i]]
        l = 0
        for s in sel[i]:
            profit += rew[s] - D[K[i]][s]
            l += ntask[s]
            asn[s] = K[i]
        loads.append(l)
    obj = profit - oc - (alphaC * (max(loads) - min(loads)) if loads else 0.0)
    return obj, asn


def phase1(tEndP, u):
    global bestObj, bestAsg
    maxK = max(1, min(nO, int(os.environ.get('MAXK', 10))))
    ext = max(max(X) - min(X), max(Y) - min(Y))
    wScale = ext * ext * 0.25
    cScale = ext * 0.25
    global servList, omax, ocost, Amax
    full_state = (servList, omax, ocost, Amax)
    SUBN = int(os.environ.get('SUBN', 1200))
    subsampled = len(servList) > SUBN
    if subsampled:
        f = SUBN / len(servList)
        servList = sorted(rnd.sample(servList, SUBN))
        omax = [max(1, int(math.ceil(m * f))) for m in omax]
        ocost = [c * f for c in ocost]
        Amax = max(1, int(Amax * f))
        u = u * 1.0
    bestScore = -INF
    bestRes = None
    bestState = None
    pInt = float(os.environ.get('PINT', 0.35))
    evals = 0
    tb = now()
    while now() < tEndP:
        intens = bestState is not None and random_() < pInt
        if intens:
            K, w, cx, cy = bestState[0][:], bestState[1][:], bestState[2][:], bestState[3][:]
            k = len(K)
        else:
            k = 1 + randrange(maxK)
            K = rnd.sample(range(nO), k)
            w = [0.0] * k
            cx = [X[nS + o] for o in K]
            cy = [Y[nS + o] for o in K]
        cur = eval_partition(K, w, cx, cy)
        if cur is None:
            continue
        curS = cur[0]
        steps = 400
        for st in range(steps):
            if now() >= tEndP:
                break
            T = u * (0.3 if intens else 3) * (0.01 / (0.3 if intens else 3)) ** (st / steps)
            K2 = K[:]
            w2 = w[:]
            cx2 = cx[:]
            cy2 = cy[:]
            r = random_()
            kk = len(K)
            if r < 0.45:
                i = randrange(kk)
                w2[i] += (random_() * 2 - 1) * wScale * (0.05 + random_() * 0.5)
            elif r < 0.75:
                i = randrange(kk)
                sc = cScale * (0.02 + random_() * 0.5)
                cx2[i] += rnd.gauss(0, sc)
                cy2[i] += rnd.gauss(0, sc)
            elif r < 0.85:
                i = randrange(kk)
                o = randrange(nO)
                if o in K2:
                    continue
                K2[i] = o
            elif r < 0.93:
                if kk >= maxK:
                    continue
                o = randrange(nO)
                if o in K2:
                    continue
                K2.append(o)
                w2.append(0.0)
                cx2.append(X[nS + o])
                cy2.append(Y[nS + o])
            else:
                if kk <= 1:
                    continue
                i = randrange(kk)
                del K2[i]
                del w2[i]
                del cx2[i]
                del cy2[i]
            nr = eval_partition(K2, w2, cx2, cy2)
            evals += 1
            if nr is None:
                continue
            if nr[0] >= curS or random_() < math.exp((nr[0] - curS) / T):
                K, w, cx, cy, curS = K2, w2, cx2, cy2, nr[0]
                if nr[0] > bestScore:
                    bestScore, bestRes = nr[0], nr[1]
                    bestState = (K[:], w[:], cx[:], cy[:])
    servList, omax, ocost, Amax = full_state
    if subsampled and bestState is not None:
        fr = eval_partition(*bestState)
        if fr is None:
            bestRes, bestScore = None, -INF
        else:
            bestScore, bestRes = fr
    print('phase1: evals=%d best=%.3f t=%.1f' % (evals, bestScore, now() - tb), file=sys.stderr)
    if bestRes is not None and bestScore > bestObj:
        c, _ = full_conflicts(bestRes)
        if c == 0:
            for s in range(nS):
                if asg[s] >= 0:
                    apply_force(s, -1)
            for s in range(nS):
                if bestRes[s] >= 0:
                    apply_force(s, bestRes[s])
            if V == 0 and count_balance_ok():
                bestObj = cur_obj()
                bestAsg = asg[:]
            print('phase1 loaded obj=%.3f V=%d' % (cur_obj(), V), file=sys.stderr)
        else:
            print('phase1 result has conflicts', c, file=sys.stderr)


def main():
    global servList, ordByDist, ordByEuc, alphaEff, lambdaV, bestObj, bestAsg
    limit = float(os.environ.get('TL', '235'))
    in_path = sys.argv[1] if len(sys.argv) > 1 else 'instance.json'
    out_path = sys.argv[2] if len(sys.argv) > 2 else 'solution.json'
    debug = 'DEBUG' in os.environ
    read_instance(in_path)
    build_grid()
    servList = [s for s in range(nS) if servable[s]]
    ordByDist = [sorted(servList, key=lambda s, o=o: D[o][s]) for o in range(nO)]
    ordByEuc = [sorted(servList, key=lambda s, o=o: (X[s] - X[nS + o]) ** 2 + (Y[s] - Y[nS + o]) ** 2) for o in range(nO)]
    build_neighbors()
    init_state()
    alphaEff = alphaC
    lambdaV = 1e6
    greedy_init()
    # unit scale u
    pr = []
    for s in servList:
        m = min(D[o][s] for o in range(nO))
        if rew[s] - m > 0:
            pr.append(rew[s] - m)
    if pr:
        pr.sort()
        u = pr[len(pr) // 2]
    else:
        u = (sum(rew[s] for s in servList) / len(servList)) if servList else 1.0
        if u <= 0:
            u = 1.0
    print('nS=%d nO=%d u=%.3f init=%.3f t=%.2f' % (nS, nO, u, bestObj, now()), file=sys.stderr)

    env = os.environ.get
    T0f, T1f = float(env('T0', 0.5)), float(env('T1', 0.01))
    L0f, L1f = float(env('L0', 0.3)), float(env('L1', 30.0))
    A0f = float(env('A0', 0.3))
    f1 = float(env('P1', 0.65))
    alphaEff = alphaC
    lambdaV = 1e9
    phase1(now() + f1 * (limit - now()), u)
    tBeg, tEnd = now(), limit
    nServ = len(servList)
    T = T0f * u
    theta_log = math.log
    iters = acc = 0
    if nServ:
        while True:
            if (iters & 63) == 0:
                t = now()
                if t > tEnd:
                    break
                p = (t - tBeg) / (tEnd - tBeg)
                T = u * T0f * (T1f / T0f) ** p
                lambdaV = u * L0f * (L1f / L0f) ** p
                alphaEff = alphaC * (A0f + (1 - A0f) * min(1.0, p / 0.5))
                if debug and (iters & 0x3FFF) == 0:
                    debug_check('loop')
                    print('t=%.1f p=%.2f V=%d obj=%.2f best=%.2f acc=%d' % (t, p, V, cur_obj(), bestObj, acc), file=sys.stderr)
            iters += 1
            r = random_()
            theta = -T * theta_log(random_() + 1e-300)
            if r < 0.62:
                s = servList[randrange(nServ)]
                kn = knn[s]
                if not kn:
                    continue
                b = asg[kn[randrange(len(kn))]]
                if b == asg[s]:
                    continue
                dE = eval_move(s, b, theta, True)
                if dE <= theta:
                    commit_pending()
                    acc += 1
                    after_accept()
            elif r < 0.90:
                s = servList[randrange(nServ)]
                a = asg[s]
                kn = knn[s]
                if a < 0 or not kn:
                    continue
                tt = kn[randrange(len(kn))]
                b = asg[tt]
                if b < 0 or b == a:
                    continue
                dE = eval_swap(s, tt, theta)
                if dE <= theta:
                    commit_pending()
                    acc += 1
                    after_accept()
            elif r < 0.997:
                s = servList[randrange(nServ)]
                r2 = random_()
                if r2 < 0.45:
                    if asg[s] >= 0:
                        b = -1
                    else:
                        b = -1
                        bp = -INF
                        for z in openList:
                            q = rew[s] - D[z][s]
                            if q > bp:
                                bp = q
                                b = z
                        if b < 0:
                            continue
                elif r2 < 0.8:
                    if not openList:
                        continue
                    b = openList[randrange(len(openList))]
                else:
                    b = min(range(nO), key=lambda z: D[z][s])
                if b == asg[s]:
                    continue
                dE = eval_move(s, b, theta, True)
                if dE <= theta:
                    commit_pending()
                    acc += 1
                    after_accept()
            else:
                rr = random_()
                if rr < 0.2:
                    macro_open(T)
                elif rr < 0.4:
                    macro_close(T)
                elif rr < 0.7:
                    macro_split(T)
                else:
                    macro_shift(T)
    print('iters=%d acc=%d best=%.3f t=%.1f' % (iters, acc, bestObj, now()), file=sys.stderr)

    # ---- verify & repair
    fin = bestAsg[:]
    for _ in range(10000):
        c, cz = full_conflicts(fin, True)
        if c == 0:
            break
        freq = {}
        for z in cz:
            freq[z] = freq.get(z, 0) + 1
        zb = max(freq, key=freq.get)
        cand = [s for s in range(nS) if fin[s] == zb]
        if not cand:
            break
        sb = min(cand, key=lambda s: rew[s] - D[zb][s])
        fin[sb] = -1
        print('repair: unassign', sb, file=sys.stderr)
    # workers
    for _ in range(100000):
        sites_of = {}
        for s in range(nS):
            if fin[s] >= 0:
                sites_of.setdefault(fin[s], []).append(s)
        ofs = sorted(sites_of)
        need = [sorted(set(k for s in sites_of[z] for k in sk[s])) for z in ofs]
        wout = assign_workers(need)
        if wout is not None:
            break
        bk, bd = -1, -10 ** 9
        for k in range(nSkill):
            c = sum(1 for nd in need if k in nd)
            if c - capSk[k] > bd:
                bd, bk = c - capSk[k], k
        bz, bc = -1, 1 << 30
        for i, z in enumerate(ofs):
            if bk in need[i]:
                c = sum(1 for s in sites_of[z] if bk in sk[s])
                if c < bc:
                    bc, bz = c, z
        if bz < 0:
            break
        for s in sites_of[bz]:
            if bk in sk[s]:
                fin[s] = -1
    res = []
    for i, z in enumerate(ofs):
        res.append({'officeId': z, 'sites': sites_of[z], 'workers': wout[i]})
    with open(out_path, 'w') as f:
        json.dump(res, f)
        f.write('\n')


if __name__ == '__main__':
    main()
