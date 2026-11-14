"""L01 / L01.1 lake layout generator - run INSIDE the Unreal editor (Python console / `py` console command):

    py "D:/Projects/LakeFight/Tools/L01_layout.py"                 -> all passes
    py "D:/Projects/LakeFight/Tools/L01_layout.py" shore beyond    -> only those passes
    py "D:/Projects/LakeFight/Tools/L01_layout.py" report          -> measurements only (no changes)

Passes: shore, beyond, gameplay, sky. Each pass deletes the actors in its outliner folder (L01/<Pass>...) and places them
again, so re-running is idempotent. Results are logged as "[L01] ..." lines in the output log. Save the level afterwards.

Shape (L01.1 option C, director): four irregular lobes. Each dock arm is rotated 13-17 deg off its diagonal, has its own
dock distance (70 m +/-5%) and lobe width (one narrow, one fat); between the arms sit four wide, rounded, wooded headlands
with tips at different radii. Gaussian bumps are solved so each bay / headland tip hits its target radius, then the outline
is smoothed in XY over SMOOTH_M metres (rounded tips, no tight pockets) and shore_r(theta) is read back from the smoothed
curve. Everything scales with SCALE.

Conventions (B03/B05 scripts, FBX Y flipped on import): shore straight = 8 m along local X, water on local +Y, waterline Z=0;
corners = R4 quarter-turn from local +X to local -Y; dock deck origin = walking surface, flag tray at (182, -108, 70).
"""
import math
import sys
import json
import unreal

SCALE = 1.0
BASE_R = 60.0
# Dock arms: angle (deg, rotated 13-17 deg off the 37/143/-143/-37 diagonals), dock water-end distance (m; measured top speed
# 6.98 m/s -> ~10 s to the centre), lobe width sigma (deg; Green narrow, Red fat).
DOCKS = {"Blue": (52.0, 72.0, 13.0), "Green": (160.0, 68.5, 10.0), "Red": (-128.0, 71.0, 17.0), "Yellow": (-50.0, 69.5, 12.0)}
DOCK_ANG = {t: v[0] for t, v in DOCKS.items()}
DOCK_RAD = {t: v[1] for t, v in DOCKS.items()}
BAY_EXTRA = 5.5                    # pre-smoothing bay target = dock end + BAY_EXTRA (smoothing pulls it back to ~+3.5)
# Headlands between the arms: angle, tip radius target (pre-smoothing), sigma (deg) - wide and rounded, all different.
HEADLANDS = [(100.0, 28.0, 21.0), (-162.0, 30.0, 18.0), (-90.0, 36.0, 12.5), (-5.0, 29.0, 20.0)]
SMOOTH_M = 7.0
SPAWN_BACK = 12.0                  # spawn this far in front of the dock end (the chase camera must not sit over the dock)
# Islands on the centre -> dock lines (searched for ~9-12 m and ~15-20 m lanes; the gameplay pass logs the measured widths).
ISLANDS = [("Blue", 27.1, 34.7, 1.1), ("Green", -35.0, 14.9, 1.1), ("Red", -24.9, -31.8, 1.1), ("Yellow", 25.0, -28.6, 1.1)]
ISLAND_WR = 5.5                    # Island_M waterline radius at scale 1 (m)
SHORE_STEP = 6.6
WATER_MARGIN = 10.0
MESH = "/Game/LakeFight/Meshes/"
GROUND = ["SM_LF_GroundPatch_01", "SM_LF_GroundPatch_02"]

ACTORS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
CM = 100.0 * SCALE
STATS = {}


def log(msg):
    unreal.log("[L01] " + msg)


class Rng:
    def __init__(self, seed): self.s = seed
    def f(self):
        self.s = (self.s * 1103515245 + 12345) % 2147483648
        return self.s / 2147483648.0
    def r(self, a, b): return a + (b - a) * self.f()
    def pick(self, seq): return seq[int(self.f() * len(seq)) % len(seq)]


def adiff(a, b):
    return (a - b + 180.0) % 360.0 - 180.0


# ---------------------------------------------------------------- shape
def r_base(t):
    return BASE_R * (1.0 + 0.03 * math.sin(3 * t + 0.7) + 0.02 * math.sin(5 * t + 2.1))


BUMPS = []  # [angle, delta, sigma, target]
for _a, _r, _s in DOCKS.values():
    BUMPS.append([_a, 0.0, _s, _r + BAY_EXTRA])
for _a, _r, _s in HEADLANDS:
    BUMPS.append([_a, 0.0, _s, _r - 2.5])


def raw_r(t):
    d = math.degrees(t)
    r = r_base(t)
    for a, delta, sig, _ in BUMPS:
        r += delta * math.exp(-0.5 * (adiff(d, a) / sig) ** 2)
    return r


for _ in range(12):
    for b in BUMPS:
        if b[3] is not None:
            b[1] += b[3] - raw_r(math.radians(b[0]))


def build_outline():
    n = 7200
    raw = [(raw_r(2 * math.pi * i / n) * math.cos(2 * math.pi * i / n), raw_r(2 * math.pi * i / n) * math.sin(2 * math.pi * i / n)) for i in range(n)]
    L = [0.0]
    for i in range(1, n + 1):
        p, q = raw[i - 1], raw[i % n]
        L.append(L[-1] + math.hypot(q[0] - p[0], q[1] - p[1]))
    per = L[-1]
    m = int(per / 0.25)
    pts, j = [], 0
    for k in range(m):
        sk = k * per / m
        while L[j + 1] < sk:
            j += 1
        p, q = raw[j], raw[(j + 1) % n]
        f = (sk - L[j]) / max(L[j + 1] - L[j], 1e-9)
        pts.append((p[0] + (q[0] - p[0]) * f, p[1] + (q[1] - p[1]) * f))
    sig = SMOOTH_M / 0.25
    rad = int(3 * sig)
    wts = [math.exp(-0.5 * (k / sig) ** 2) for k in range(-rad, rad + 1)]
    wsum = sum(wts)
    out = []
    for i in range(m):
        x = y = 0.0
        for k, w in zip(range(-rad, rad + 1), wts):
            P = pts[(i + k) % m]
            x += P[0] * w
            y += P[1] * w
        out.append((x / wsum, y / wsum))
    return out


OUT = build_outline()
N = len(OUT)
TAB = sorted((math.atan2(p[1], p[0]), math.hypot(p[0], p[1])) for p in OUT)


def shore_r(t):
    """Shoreline radius of the SMOOTHED outline along the ray at angle t (radians); the lake is star-shaped from the centre."""
    t = math.atan2(math.sin(t), math.cos(t))
    if t <= TAB[0][0] or t >= TAB[-1][0]:
        a0, r0 = TAB[-1]
        a1, r1 = TAB[0][0] + 2 * math.pi, TAB[0][1]
        tt = t if t >= TAB[-1][0] else t + 2 * math.pi
        return r0 + (r1 - r0) * (tt - a0) / max(a1 - a0, 1e-9)
    lo, hi = 0, len(TAB) - 1
    while hi - lo > 1:
        mid = (lo + hi) // 2
        if TAB[mid][0] <= t: lo = mid
        else: hi = mid
    (a0, r0), (a1, r1) = TAB[lo], TAB[hi]
    return r0 + (r1 - r0) * (t - a0) / max(a1 - a0, 1e-9)


def polar(ang_deg, r):
    t = math.radians(ang_deg)
    return (r * math.cos(t), r * math.sin(t))


def inland(x, y):
    return math.hypot(x, y) - shore_r(math.atan2(y, x))


ARC = [0.0]
for _i in range(1, N + 1):
    _p, _q = OUT[_i - 1], OUT[_i % N]
    ARC.append(ARC[-1] + math.hypot(_q[0] - _p[0], _q[1] - _p[1]))
PERIM = ARC[-1]


def at_arc(s):
    s %= PERIM
    lo, hi = 0, N
    while hi - lo > 1:
        mid = (lo + hi) // 2
        if ARC[mid] <= s: lo = mid
        else: hi = mid
    p, q = OUT[lo], OUT[(lo + 1) % N]
    f = (s - ARC[lo]) / max(ARC[lo + 1] - ARC[lo], 1e-6)
    x, y = p[0] + (q[0] - p[0]) * f, p[1] + (q[1] - p[1]) * f
    tx, ty = q[0] - p[0], q[1] - p[1]
    l = math.hypot(tx, ty) or 1.0
    return x, y, tx / l, ty / l


def frame_at_angle(ang_deg):
    t = math.radians(ang_deg)
    x, y = polar(ang_deg, shore_r(t))
    x2, y2 = polar(ang_deg + 0.1, shore_r(t + math.radians(0.1)))
    l = math.hypot(x2 - x, y2 - y)
    return x, y, (x2 - x) / l, (y2 - y) / l


def island_lanes(cx, cy, team):
    """Lane widths either side of an island: island waterline -> nearest shoreline, per side of the centre->dock diagonal."""
    a = math.radians(DOCK_ANG[team])
    d = (math.cos(a), math.sin(a))
    n = (-d[1], d[0])
    w = [i for i in ISLANDS if i[0] == team][0][3] * ISLAND_WR
    lanes = {}
    for P in OUT:
        vx, vy = P[0] - cx, P[1] - cy
        if abs(vx * d[0] + vy * d[1]) > 14.0:
            continue
        side = 1 if vx * n[0] + vy * n[1] > 0 else -1
        lanes[side] = min(lanes.get(side, 1e9), math.hypot(vx, vy) - w)
    return lanes, w, d, n


# ---------------------------------------------------------------- editor helpers
def load(name):
    a = unreal.EditorAssetLibrary.load_asset(MESH + name)
    if not a:
        raise RuntimeError("missing asset " + MESH + name)
    return a


def add(mesh, label, x, y, z, yaw=0.0, sc=(1.0, 1.0, 1.0), folder="L01", cls=None):
    loc = unreal.Vector(x * CM, y * CM, z * 100.0)
    rot = unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw)
    a = ACTORS.spawn_actor_from_class(cls, loc, rot) if cls else ACTORS.spawn_actor_from_object(load(mesh), loc, rot)
    a.set_actor_scale3d(unreal.Vector(sc[0], sc[1], sc[2]))
    a.set_actor_label(label)
    a.set_folder_path(folder)
    key = mesh or cls.__name__
    STATS[key] = STATS.get(key, 0) + 1
    return a


def all_actors():
    return ACTORS.get_all_level_actors()


def clear(prefix):
    n = 0
    for a in all_actors():
        f = str(a.get_folder_path())
        if f == prefix or f.startswith(prefix + "/"):
            ACTORS.destroy_actor(a)
            n += 1
    return n


def find(label):
    for a in all_actors():
        if a.get_actor_label() == label:
            return a
    return None


def place_xy(a, x, y, z_cm, yaw):
    a.set_actor_location_and_rotation(unreal.Vector(x * CM, y * CM, z_cm), unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw), False, True)


# ---------------------------------------------------------------- passes
def pass_shore():
    removed = clear("L01/Shore")
    gone = []
    for lab in ["Shore_N", "Shore_S", "Shore_E", "Shore_W", "Dock_Deck", "Dock_Post_A", "Dock_Post_B"]:
        a = find(lab)
        if a:
            ACTORS.destroy_actor(a)
            gone.append(lab)
    xs = [p[0] for p in OUT]
    ys = [p[1] for p in OUT]
    sx = (max(-min(xs), max(xs)) + WATER_MARGIN) * 2.0
    sy = (max(-min(ys), max(ys)) + WATER_MARGIN) * 2.0
    lake = find("LF_Lake")
    lake.set_editor_property("Size", unreal.Vector2D(sx * CM, sy * CM))
    bed = find("LakeBed")
    bed.set_actor_location(unreal.Vector(0, 0, -450), False, True)
    bed.set_actor_scale3d(unreal.Vector(sx * SCALE + 4, sy * SCALE + 4, 1))
    n = int(PERIM / SHORE_STEP) + 1
    for i in range(n):
        x, y, tx, ty = at_arc(i * PERIM / n)
        add("SM_LF_Shore_Straight", "Shore_%03d" % i, x, y, 0.0, math.degrees(math.atan2(ty, tx)), folder="L01/Shore")
    # No CornerOut on the headland tips (L01.1): their R4 quarter-turn left squared notches on the wide rounded headlands.
    # CornerIn at the back of each dock bay, beside the dock (water inside the arc).
    for k, (team, ang) in enumerate(DOCK_ANG.items()):
        a2 = ang + (9.0 if k % 2 else -9.0)
        x, y, tx, ty = frame_at_angle(a2)
        nx, ny = -ty, tx
        add("SM_LF_Shore_CornerIn", "Shore_CornerIn_%s" % team, x + 4 * nx / SCALE, y + 4 * ny / SCALE, 0.0,
            math.degrees(math.atan2(-ny, -nx)) + 45.0, folder="L01/Shore")
    log("shore: removed %d (+pool %s), perimeter %.1f m, %d straights, outline %.1f x %.1f m, water %.1f x %.1f m"
        % (removed, gone, PERIM, n, max(xs) - min(xs), max(ys) - min(ys), sx, sy))


def pass_beyond():
    removed = clear("L01/Beyond")
    rng = Rng(20261114)
    F = "L01/Beyond"

    skipped = [0]

    def tile(label, x, y, z, s, zs, base_yaw=0.0):
        # _01 / _02 ~50/50, random 90-degree yaw, so the ground doesn't repeat (L01.1; _02 normals fixed in 3ccb158).
        # The tiles have collision now (L01.1): never let one overhang the water - all 9 points of its square must be on land.
        yaw = base_yaw + rng.pick([0, 90, 180, 270])
        h = 20.0 * s
        c, sn = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
        for dx in (-1, 0, 1):
            for dy in (-1, 0, 1):
                px, py = x + (dx * c - dy * sn) * h, y + (dx * sn + dy * c) * h
                if inland(px, py) < 0.0:
                    skipped[0] += 1
                    return False
        add(rng.pick(GROUND), label, x, y, z, yaw, (s * SCALE, s * SCALE, zs), F + "/Ground")
        rec = (x, y, h, c, sn)
        rr = h * 1.42
        for bx in range(int(math.floor((x - rr) / 10.0)), int(math.floor((x + rr) / 10.0)) + 1):
            for by in range(int(math.floor((y - rr) / 10.0)), int(math.floor((y + rr) / 10.0)) + 1):
                buckets.setdefault((bx, by), []).append(rec)
        return True

    buckets = {}  # 10 m spatial hash of placed tiles (coverage checks for the gap fills)

    def covered_pt(px, py):
        for tx, ty, th, tc, ts in buckets.get((int(math.floor(px / 10.0)), int(math.floor(py / 10.0))), ()):
            dx, dy = px - tx, py - ty
            if abs(dx * tc + dy * ts) <= th + 0.05 and abs(-dx * ts + dy * tc) <= th + 0.05:
                return True
        return False

    tiles = 0
    for row, off in enumerate([10.0, 24.0]):
        n = int((PERIM + 2 * math.pi * off) / 14.0)
        for i in range(n):
            x, y, tx, ty = at_arc(i * PERIM / n)
            nx, ny = -ty, tx
            if inland(x - nx * off, y - ny * off) < 3.0:
                continue  # narrow peninsula: the offset point crossed into the other side's water
            tiles += tile("Ground_B%d_%03d" % (row, i), x - nx * off, y - ny * off, 0.42 - 0.01 * row, 0.45, 0.5,
                          math.degrees(math.atan2(ty, tx)))

    def beyond(cx, cy, half, margin):
        return all(inland(cx + dx * half, cy + dy * half) > margin for dx in (-1, 0, 1) for dy in (-1, 0, 1))

    ext = 160.0
    covered = []
    g = -ext
    gi = 0
    while g < ext:
        h = -ext
        while h < ext:
            cx, cy = g + 20.0, h + 20.0
            if beyond(cx, cy, 20.0, 4.0):
                tiles += tile("Ground_G%03d" % gi, cx, cy, 0.38, 1.0, 1.0); gi += 1
                covered.append((cx, cy, 20.0))
            else:
                for qx in (-10.0, 10.0):
                    for qy in (-10.0, 10.0):
                        if beyond(cx + qx, cy + qy, 10.0, 4.0):
                            tiles += tile("Ground_G%03d" % gi, cx + qx, cy + qy, 0.39, 0.5, 0.7); gi += 1
                            covered.append((cx + qx, cy + qy, 10.0))
            h += 40.0
        g += 40.0
    for ix in range(int(-ext / 10), int(ext / 10)):
        for iy in range(int(-ext / 10), int(ext / 10)):
            cx, cy = ix * 10.0 + 5.0, iy * 10.0 + 5.0
            if any(abs(cx - kx) < kh and abs(cy - ky) < kh for kx, ky, kh in covered):
                continue
            if beyond(cx, cy, 5.0, 1.0):
                tiles += tile("Ground_S%03d" % gi, cx, cy, 0.40, 0.27, 0.4); gi += 1
    for gx in range(-4, 4):
        for gy in range(-4, 4):
            cx, cy = gx * 80.0 + 40.0, gy * 80.0 + 40.0
            if max(abs(cx), abs(cy)) - 40.0 >= ext - 1.0:
                tiles += tile("Ground_F%03d" % gi, cx, cy, 0.36, 2.0, 1.5); gi += 1
    # 160 m ring out to 640 m: the far hills now stand 420-570 m out.
    for gx in range(-4, 4):
        for gy in range(-4, 4):
            cx, cy = gx * 160.0 + 80.0, gy * 160.0 + 80.0
            if max(abs(cx), abs(cy)) - 80.0 >= 319.0:
                tiles += tile("Ground_X%03d" % gi, cx, cy, 0.34, 4.0, 2.0); gi += 1
    # Final gap fill (L01.1): small 5 m tiles wherever land inside the water plane's rectangle is still uncovered (peninsula necks,
    # skipped band tiles) - the water plane would show through there.
    gaps = 0
    hx = hy = ext  # the whole 40 m grid area (director: dark blue triangles scattered in the ground too)
    gx = -hx + 1.25  # half-step offset: never sample exactly on the seam between two grid-aligned tiles
    while gx <= hx:
        gy = -hy + 1.25
        while gy <= hy:
            if inland(gx, gy) > 0.4 and not covered_pt(gx, gy):
                if tile("Ground_Gap%03d" % gaps, gx, gy, 0.41, 0.15, 0.3):
                    gaps += 1
            gy += 2.5
        gx += 2.5
    # Second fill with 3 m tiles: the narrow headland tips had no room for a 6 m tile without overhanging the water.
    gx = -hx + 0.6
    while gx <= hx:
        gy = -hy + 0.6
        while gy <= hy:
            if inland(gx, gy) > 0.4 and not covered_pt(gx, gy):
                if tile("Ground_Gap%03d" % gaps, gx, gy, 0.415, 0.075, 0.2):
                    gaps += 1
            gy += 1.25
        gx += 1.25
    tiles += gaps
    log("beyond: gap-fill tiles %d" % gaps)
    trees = 0
    for i in range(80):
        ang = rng.r(0, 360)
        t = math.radians(ang)
        r = shore_r(t) + rng.r(8.0, 30.0)
        x, y = polar(ang, r)
        if any(abs(adiff(ang, d)) < 9 for d in DOCK_ANG.values()) or inland(x, y) < 6.0:
            continue
        add(rng.pick(["SM_LF_Tree_Pine", "SM_LF_Tree_Pine", "SM_LF_Tree_Round"]), "Tree_%03d" % i, x, y, 0.4, rng.r(0, 360),
            (rng.r(0.8, 1.25),) * 3, F + "/Forest"); trees += 1
    for i in range(130):
        u = rng.f() ** 0.6
        ang = rng.r(0, 360)
        t = math.radians(ang)
        r = shore_r(t) + 25.0 + u * 90.0
        x, y = polar(ang, r)
        if (any(abs(adiff(ang, d)) < 6 for d in DOCK_ANG.values()) and r < shore_r(t) + 35) or inland(x, y) < 12.0:
            continue
        s = rng.r(0.9, 1.4)
        add("SM_LF_TreeCluster_01", "TreeCluster_%03d" % i, x, y, 0.35, rng.r(0, 360), (s, s, s * rng.r(0.9, 1.2)), F + "/Forest"); trees += 1
    # Headlands: wooded (trees + rocks along and across each one) so they block the sight lines between neighbouring docks.
    pen = 0
    for k, (ang, _r, _s) in enumerate(HEADLANDS):
        tip = shore_r(math.radians(ang))
        for j2, (da, off) in enumerate([(-9.0, 9.0), (8.0, 10.0), (-12.0, 17.0), (11.0, 19.0), (0.0, 34.0)]):
            x, y = polar(ang + da, tip + off)
            if inland(x, y) < 5.0:
                continue
            add(rng.pick(["SM_LF_Tree_Pine", "SM_LF_Tree_Round", "SM_LF_TreeCluster_01"]), "Head%d_Wood%d" % (k, j2), x, y, 0.35,
                rng.r(0, 360), (0.8, 0.8, 0.9) if j2 == 4 else (1.25, 1.25, 1.25), F + "/Headlands")
            pen += 1
        for j, off in enumerate([5.0, 11.0, 18.0, 26.0]):
            x, y = polar(ang + rng.r(-2.0, 2.0), tip + off)
            if j == 0:
                add(rng.pick(["SM_LF_Rock_02", "SM_LF_Rock_03"]), "Head%d_Rock" % k, x, y, 0.35, rng.r(0, 360), (1.2, 1.2, 1.2), F + "/Headlands")
                xb, yb = polar(ang + 6.0, tip + 3.0)
                add("SM_LF_Bush_01", "Head%d_Bush" % k, xb, yb, 0.35, rng.r(0, 360), folder=F + "/Headlands")
            elif j == 1:
                add(rng.pick(["SM_LF_Tree_Pine", "SM_LF_Tree_Round"]), "Head%d_Tree" % k, x, y, 0.35, rng.r(0, 360), (1.3, 1.3, 1.3), F + "/Headlands")
            else:
                add("SM_LF_TreeCluster_01", "Head%d_Cluster%d" % (k, j), x, y, 0.35, rng.r(0, 360), (0.7, 0.7, 0.9), F + "/Headlands")
            pen += 1
    clumps = 0
    for i in range(14):
        ang = rng.r(0, 360)
        if any(abs(adiff(ang, d)) < 7 for d in DOCK_ANG.values()):
            continue
        t = math.radians(ang)
        r0 = shore_r(t) + rng.r(4.0, 10.0)
        for j in range(int(rng.r(3, 6))):
            x, y = polar(ang + rng.r(-2.5, 2.5), r0 + rng.r(-2.0, 3.0))
            if inland(x, y) < 3.0:
                continue
            m = rng.pick(["SM_LF_Rock_01", "SM_LF_Rock_02", "SM_LF_Rock_03", "SM_LF_Bush_01", "SM_LF_Bush_02", "SM_LF_Bush_01"])
            add(m, "Clump_%02d_%d" % (i, j), x, y, 0.35, rng.r(0, 360), (rng.r(0.7, 1.3),) * 3, F + "/Clumps")
        clumps += 1
    # Far hills (L01.1): fewer, lower (Z 0.8-1.1), wider, 1.5x further out (420-570 m), overlapping.
    hills = 0
    for i in range(7):
        ang = i * (360.0 / 7) + rng.r(-8, 8)
        x, y = polar(ang, rng.r(420.0, 570.0))
        s = rng.r(1.6, 2.0)
        add(rng.pick(["SM_LF_Hill_Far_01", "SM_LF_Hill_Far_02", "SM_LF_Hill_Far_03"]), "Hill_%d" % i, x, y, 0.0, ang + 90.0,
            (s * SCALE, s * SCALE, rng.r(0.8, 1.1) * SCALE), F + "/Hills"); hills += 1
    log("beyond: ground tiles skipped (would overhang water) %d" % skipped[0])
    log("beyond: removed %d, ground tiles %d, trees %d, headland pieces %d, clumps %d, hills %d" % (removed, tiles, trees, pen, clumps, hills))


def reed_patch(name, cx, cy, ra, rb, rng, folder):
    """Organic reed patch (director: the 4 x 3 m ReedBed meshes read as squares): SM_LF_Reeds clumps (1.2 m) scattered on a
    jittered 0.75 m grid inside a noisy, rotated ellipse (ra x rb m), taller in the middle, plus a few stragglers outside the edge.
    Returns the patch's lake-side edge point (for the heron)."""
    rot = math.radians(rng.r(0, 180))
    p1, p2 = rng.r(0, 6.3), rng.r(0, 6.3)
    cr, sr = math.cos(rot), math.sin(rot)

    def edge(phi):
        return 1.0 + 0.22 * math.sin(2 * phi + p1) + 0.14 * math.sin(3 * phi + p2)

    n = 0
    gx = -ra * 1.4
    while gx <= ra * 1.4:
        gy = -rb * 1.4
        while gy <= rb * 1.4:
            lx, ly = gx + rng.r(-0.35, 0.35), gy + rng.r(-0.35, 0.35)
            phi = math.atan2(ly / rb, lx / ra)
            d = math.hypot(lx / ra, ly / rb) / edge(phi)
            if d <= 1.0 or (d <= 1.25 and rng.f() < 0.18):
                x, y = cx + lx * cr - ly * sr, cy + lx * sr + ly * cr
                if inland(x, y) < -1.0:  # stay in the water (shallows)
                    sc = max(0.65, 1.35 - 0.55 * d) * rng.r(0.85, 1.15)
                    add("SM_LF_Reeds", "%s_%02d" % (name, n), x, y, -0.1, rng.r(0, 360), (sc, sc, sc * rng.r(0.9, 1.2)), folder)
                    n += 1
            gy += 0.75
        gx += 0.75
    return n


def pass_gameplay():
    removed = clear("L01/Gameplay")
    rng = Rng(777)
    F = "L01/Gameplay"
    docks = {}
    for team, ang in DOCK_ANG.items():
        x, y = polar(ang, DOCK_RAD[team] + 2.0)
        yaw = ang + 180.0
        add("SM_LF_Dock_Team", "Dock_%s" % team, x, y, 0.5, yaw, folder=F + "/Docks")
        t = math.radians(yaw)
        add("SM_LF_TeamFlag_%s" % team, "Flag_%s" % team, x + (1.82 * math.cos(t) + 1.08 * math.sin(t)) / SCALE,
            y + (1.82 * math.sin(t) - 1.08 * math.cos(t)) / SCALE, 1.2, yaw, folder=F + "/Docks")
        bx, by = polar(ang, DOCK_RAD[team] - 4.0 / SCALE)
        add("SM_LF_BankZone", "BankZone_%s" % team, bx, by, 0.02, yaw, folder=F + "/Docks")
        docks[team] = (x, y)
        sx, sy = polar(ang + 6.0, shore_r(math.radians(ang + 6.0)) + 1.5)
        add("SM_LF_Log", "DockLog_%s" % team, sx, sy, 0.05, ang + 75.0, folder=F + "/Dressing")
        bx, by = polar(ang - 7.0, shore_r(math.radians(ang - 7.0)) + 5.0)
        add("SM_LF_Bush_02", "DockBush_%s" % team, bx, by, 0.4, rng.r(0, 360), folder=F + "/Dressing")
        for k in range(2):
            ex, ey = polar(ang + (k - 0.5) * 9.0, shore_r(math.radians(ang)) + 6.5)
            add("SM_LF_Fence_Section", "DockFence_%s_%d" % (team, k), ex, ey, 0.45, ang + 90.0, folder=F + "/Dressing")
    # Spawns: 12 m in front of each dock (the chase camera must not sit over the dock), bow to the centre.
    for team, ang in DOCK_ANG.items():
        x, y = polar(ang, DOCK_RAD[team] - SPAWN_BACK)
        yaw = ang + 180.0
        if team == "Red":
            place_xy(find("PlayerStart"), x, y, 40.0, yaw)
        else:
            place_xy(find("LF_Boat_" + team), x, y, 0.0, yaw)
            place_xy(find("LF_Crew_" + team), x, y, 80.0, yaw)
    # Islands on the routes home (12-18 m across), dressed to block sight lines; a reed bed in each wide lane.
    lanes_out = {}
    lane_pts = {}
    reed_count = [0]
    for team, cx, cy, s in ISLANDS:
        add("SM_LF_Island_M", "Island_%s" % team, cx, cy, 0.0, rng.r(0, 360), (s, s, s), F + "/Islands")
        top = 0.65 * s
        add("SM_LF_Tree_Pine", "Island_%s_Pine" % team, cx - 1.4 * s, cy + 1.0 * s, top, rng.r(0, 360), (1.35, 1.35, 1.35), F + "/Islands")
        add(rng.pick(["SM_LF_Tree_Round", "SM_LF_Tree_Pine"]), "Island_%s_Tree" % team, cx + 1.6 * s, cy - 1.1 * s, top - 0.1, rng.r(0, 360),
            (1.15, 1.15, 1.15), F + "/Islands")
        add(rng.pick(["SM_LF_Rock_02", "SM_LF_Rock_03"]), "Island_%s_Rock" % team, cx + 0.4 * s, cy + 2.4 * s, top - 0.35, rng.r(0, 360),
            (1.0, 1.0, 1.1), F + "/Islands")
        add("SM_LF_Bush_02", "Island_%s_Bush" % team, cx - 2.2 * s, cy - 1.8 * s, top - 0.25, rng.r(0, 360), folder=F + "/Islands")
        L, w, d, n = island_lanes(cx, cy, team)
        narrow_side = 1 if L.get(1, 99) < L.get(-1, 99) else -1
        lanes_out[team] = {"narrow_m": round(L[narrow_side], 1), "wide_m": round(L[-narrow_side], 1), "dia_m": round(2 * w, 1),
                           "r_m": round(math.hypot(cx, cy), 1)}
        for side, kind in ((narrow_side, "narrow"), (-narrow_side, "wide")):
            mid = w + L[side] * 0.5
            lane_pts[(team, kind)] = (cx + n[0] * side * mid, cy + n[1] * side * mid)
        # Reed patch in the wide lane (pass-through lurk spot) - organic clump patches, not the square ReedBed meshes (L01.1).
        rx, ry = lane_pts[(team, "wide")]
        reed_count[0] += reed_patch("Reeds_Lane_%s" % team, rx, ry, rng.r(3.0, 4.2), rng.r(2.0, 2.8), rng, F + "/Reeds")
    for team in ("Green", "Yellow"):
        ang = DOCK_ANG[team] + (10.0 if team == "Green" else -10.0)
        x, y = polar(ang, shore_r(math.radians(ang)) - 4.5)
        reed_count[0] += reed_patch("Reeds_Bay_%s" % team, x, y, rng.r(3.5, 4.5), rng.r(2.0, 2.6), rng, F + "/Reeds")
    # Heron at the lake-side edge of the Blue lane reed patch, feet at the waterline.
    hx, hy = lane_pts[("Blue", "wide")]
    hd = math.hypot(hx, hy)
    add("SM_LF_Heron", "Heron", hx - hx / hd * 4.0, hy - hy / hd * 4.0, 0.0, math.degrees(math.atan2(-hy, -hx)) + 35.0, folder=F + "/Wildlife")
    log("gameplay: reed patches 6, %d SM_LF_Reeds clumps (no ReedBed meshes)" % reed_count[0])
    for k, ang in enumerate([60, 120, 200, 245, 300, 330]):
        x, y = polar(ang, shore_r(math.radians(ang)) - rng.r(3.0, 5.0))
        add("SM_LF_LilyPads", "LilyPads_%d" % k, x, y, 0.03, rng.r(0, 360), (1.2, 1.2, 1.0), F + "/Dressing")
    for k, ang in enumerate([100, 165, 225, 315]):
        x, y = polar(ang, shore_r(math.radians(ang)) - 3.0)
        add("SM_LF_Stump", "Stump_%d" % k, x, y, -0.05, rng.r(0, 360), folder=F + "/Dressing")
    for team, cx, cy, s in ISLANDS:
        x, y = lane_pts[(team, "narrow")]
        add("SM_LF_Buoy", "Buoy_%s_narrow" % team, x, y, 0.0, rng.r(0, 360), folder=F + "/Dressing")
    sa = DOCK_ANG["Blue"] + 13.0
    x, y = polar(sa, shore_r(math.radians(sa)) + 7.0)
    add("SM_LF_Shack", "Shack", x, y, 0.45, sa + 90.0, folder=F + "/Shack")
    for k in range(2):
        cx, cy = polar(sa - 4.0 + k * 0.8, shore_r(math.radians(sa)) + 4.0)
        add("SM_LF_Crate", "Shack_Crate_%d" % k, cx, cy, 0.45, rng.r(0, 360), folder=F + "/Shack")
    for k in range(3):
        ex, ey = polar(sa + 4.5 + k * 1.6, shore_r(math.radians(sa)) + 9.0)
        add("SM_LF_Fence_Section", "Shack_Fence_%d" % k, ex, ey, 0.45, sa + 90.0 + 12.0 * k, folder=F + "/Shack")
    lx, ly = polar(sa + 6.0, shore_r(math.radians(sa + 6.0)) + 4.0)
    add("SM_LF_Log", "Shack_Log", lx, ly, 0.4, sa, folder=F + "/Shack")
    bx, by = polar(sa - 7.0, shore_r(math.radians(sa)) + 8.0)
    add("SM_LF_Bush_01", "Shack_Bush", bx, by, 0.4, folder=F + "/Shack")
    # Fish zones: centre + two island lanes on opposite sides of the lake (>= 140 deg apart around the centre), the pair that
    # makes each dock's nearest lane zone as equal as possible (fair to all four docks).
    best = None
    keys = list(lane_pts.keys())
    for i1 in range(len(keys)):
        for i2 in range(i1 + 1, len(keys)):
            (t1, k1), (t2, k2) = keys[i1], keys[i2]
            if t1 == t2:
                continue
            z1, z2 = lane_pts[keys[i1]], lane_pts[keys[i2]]
            if abs(adiff(math.degrees(math.atan2(z1[1], z1[0])), math.degrees(math.atan2(z2[1], z2[0])))) < 140.0:
                continue
            near = {t: min(math.hypot(zx - dx, zy - dy) for zx, zy in (z1, z2)) for t, (dx, dy) in docks.items()}
            spread = max(near.values()) - min(near.values())
            if best is None or spread < best[0]:
                best = (spread, (t1, t2), (k1, k2), (z1, z2), near)
    zones = {"FishZone_Centre": (0.0, 0.0),
             "FishZone_%s_%s" % (best[1][0], best[2][0]): best[3][0],
             "FishZone_%s_%s" % (best[1][1], best[2][1]): best[3][1]}
    for lab, (x, y) in zones.items():
        add(None, lab, x, y, 0.5, cls=unreal.TargetPoint, folder=F + "/FishZones")
    log("gameplay: removed %d; lanes %s" % (removed, json.dumps(lanes_out)))
    log("gameplay: fish zones %s; nearest lane zone per dock %s (spread %.1f m)" % (
        json.dumps({k: [round(v[0], 1), round(v[1], 1)] for k, v in zones.items()}), json.dumps({k: round(v, 1) for k, v in best[4].items()}), best[0]))
    log("gameplay: dock water ends %s m from the centre (%s s at 6.98 m/s); dock bay shoreline %s m" % (
        json.dumps(DOCK_RAD), json.dumps({t: round(r / 6.98, 1) for t, r in DOCK_RAD.items()}),
        json.dumps({t: round(shore_r(math.radians(a)), 1) for t, a in DOCK_ANG.items()})))


def pass_sky():
    removed = clear("L01/Sky")
    rng = Rng(1114)
    for i in range(10):
        ang = i * 36.0 + rng.r(-12, 12)
        r, z, s = rng.r(250, 400), rng.r(80, 150) + 25.0, rng.r(1.3, 3.0)  # L01.1: lifted 25 m
        m = rng.pick(["SM_LF_Cloud_01", "SM_LF_Cloud_02", "SM_LF_Cloud_03", "SM_LF_Cloud_04"])
        x, y = polar(ang, r)
        add(m, "Cloud_%d" % i, x / SCALE, y / SCALE, z, rng.r(0, 360), (s, s, s), "L01/Sky")
    log("sky: removed %d, clouds 10 (105-175 m up)" % removed)


def report():
    xs = [p[0] for p in OUT]
    ys = [p[1] for p in OUT]
    tips = {}
    for a, _r, _s in HEADLANDS:
        tips[a] = round(min(math.hypot(*p) for p in OUT if abs(adiff(math.degrees(math.atan2(p[1], p[0])), a)) < 20), 1)
    log("report: outline %.1f x %.1f m, perimeter %.1f m, open centre radius %.1f m, headland tips %s" % (
        max(xs) - min(xs), max(ys) - min(ys), PERIM, min(math.hypot(*p) for p in OUT), json.dumps(tips)))


def run(passes):
    if "shore" in passes: pass_shore()
    if "beyond" in passes: pass_beyond()
    if "gameplay" in passes: pass_gameplay()
    if "sky" in passes: pass_sky()
    report()
    log("placed %s" % json.dumps(STATS))


run(sys.argv[1:] or ["shore", "beyond", "gameplay", "sky"])
