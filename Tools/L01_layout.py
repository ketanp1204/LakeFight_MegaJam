"""L01 lake layout generator (run through the Unreal MCP ProgrammaticToolset: execute_tool_script).

Idempotent per pass: each pass first deletes the actors in its outliner folder (L01/<Pass>), then places them again.
Everything is derived from a few numbers at the top, so the whole lake can be re-scaled in a later pass:
  SCALE      - multiplies every distance (1.0 = the L01 review layout)
  A_ELL/B_ELL- base ellipse semi-axes of the shoreline (m)
  DOCK_R     - dock water end distance from the lake centre (m)
Conventions (from the B03/B05 scripts, FBX Y flipped on import): shore straight = 8 m along local X, water on local +Y,
waterline Z=0; corners = R4 quarter-turn from local +X to local -Y; dock deck origin = walking surface, flag tray at
(182, -108, 70). Usage: set PASS to "shore", "beyond" or "gameplay" (or "all") and paste into execute_tool_script.
"""
import json, math

PASS = "all"
SCALE = 1.0
A_ELL, B_ELL = 86.0, 58.0
DOCK_R = 70.0  # measured top speed 6.98 m/s -> 10.0 s dock -> centre
DOCK_ANG = {"Blue": 37.0, "Green": 143.0, "Red": -143.0, "Yellow": -37.0}
POINTS = {0.0: 68.0, 180.0: 68.0, 270.0: 48.0}          # shore points (angle: shoreline radius)
SMALL_BAYS = {112.0: 4.0, -72.0: 4.0}                    # decorative concave bays (angle: extra depth)
ISLANDS = [("N", 90.0, 34.0, "SM_LF_Island_M", 5.5), ("E", 0.0, 46.0, "SM_LF_Island_S", 2.5), ("W", 180.0, 46.0, "SM_LF_Island_S", 2.5)]
SHORE_STEP = 6.6
WATER_MARGIN = 10.0
LEVEL = "/Game/LakeFight/Maps/L_Lake01.L_Lake01:PersistentLevel."
MESH = "/Game/LakeFight/Meshes/"

SC = "editor_toolset.toolsets.scene.SceneTools."
AT = "editor_toolset.toolsets.actor.ActorTools."
OT = "editor_toolset.toolsets.object.ObjectTools."
CM = 100.0 * SCALE


class Rng:
    def __init__(self, seed): self.s = seed
    def f(self):
        self.s = (self.s * 1103515245 + 12345) % 2147483648
        return self.s / 2147483648.0
    def r(self, a, b): return a + (b - a) * self.f()
    def pick(self, seq): return seq[int(self.f() * len(seq)) % len(seq)]


def adiff(a, b):  # degrees
    return (a - b + 180.0) % 360.0 - 180.0


def r_base(t):
    c, s = math.cos(t), math.sin(t)
    r = 1.0 / math.sqrt(c * c / (A_ELL * A_ELL) + s * s / (B_ELL * B_ELL))
    return r * (1.0 + 0.025 * math.sin(3 * t + 0.7) + 0.015 * math.sin(5 * t + 2.1))


# Gaussian bumps solved so the shoreline hits its targets exactly (docks sit in a bay, points stick out).
BUMPS = []  # [angle_deg, delta_m, sigma_deg, target_or_None]
for name, ang in DOCK_ANG.items():
    BUMPS.append([ang, 0.0, 7.0, DOCK_R + 3.5])
    for side in (-1.0, 1.0):  # small headlands framing each dock's bay
        BUMPS.append([ang + side * 15.0, -4.0, 5.0, None])
for ang, tr in POINTS.items():
    BUMPS.append([ang, 0.0, 9.0 if ang in (0.0, 180.0) else 14.0, tr])
for ang, depth in SMALL_BAYS.items():
    BUMPS.append([ang, depth, 4.5, None])


def shore_r(t):
    d = math.degrees(t)
    r = r_base(t)
    for ang, delta, sig, _ in BUMPS:
        r += delta * math.exp(-0.5 * (adiff(d, ang) / sig) ** 2)
    return r


for _ in range(6):
    for b in BUMPS:
        if b[3] is not None:
            b[1] += b[3] - shore_r(math.radians(b[0]))


def polar(ang_deg, r):
    t = math.radians(ang_deg)
    return (r * math.cos(t), r * math.sin(t))


def shore_pt(t):
    r = shore_r(t)
    return (r * math.cos(t), r * math.sin(t))


def inland(x, y):  # metres beyond the shoreline along the ray from the centre (negative = in the water)
    return math.hypot(x, y) - shore_r(math.atan2(y, x))


# ---------------------------------------------------------------- MCP helpers
STATS = {}


def add(mesh, label, x, y, z, yaw=0.0, sc=(1.0, 1.0, 1.0), folder="L01", cls=None, pitch=0.0, roll=0.0):
    xf = {"location": {"x": x * CM, "y": y * CM, "z": z * 100.0}, "rotation": {"pitch": pitch, "yaw": yaw, "roll": roll},
          "scale": {"x": sc[0], "y": sc[1], "z": sc[2]}}
    if cls:
        a = execute_tool(SC + "add_to_scene_from_class", json.dumps({"actor_type": {"refPath": cls}, "name": label, "xform": xf}))["returnValue"]
    else:
        a = execute_tool(SC + "add_to_scene_from_asset", json.dumps({"asset_path": MESH + mesh, "name": label, "xform": xf}))["returnValue"]
    execute_tool(AT + "set_label", json.dumps({"actor": a, "label": label}))
    execute_tool(SC + "set_actor_folder", json.dumps({"actor": a, "folder_path": folder}))
    key = mesh or cls.split(".")[-1]
    STATS[key] = STATS.get(key, 0) + 1
    return a


def clear(folder):
    if folder not in execute_tool(SC + "get_folders", "{}")["returnValue"]:
        return 0
    acts = execute_tool(SC + "get_actors_in_folder", json.dumps({"folder_path": folder, "recursive": True}))["returnValue"]
    for a in acts:
        execute_tool(SC + "remove_from_scene", json.dumps({"actor": a}))
    return len(acts)


def setp(obj, values):
    return execute_tool(OT + "set_properties", json.dumps({"instance": obj, "values": json.dumps(values)}))["returnValue"]


def find(label):
    for a in execute_tool(SC + "find_actors", json.dumps({"name": label, "tag": "", "collision_channels": []}))["returnValue"]:
        if execute_tool(AT + "get_label", json.dumps({"actor": a}))["returnValue"] == label:
            return a
    return None


# ---------------------------------------------------------------- outline sampling
N = 2400
OUT = [shore_pt(2 * math.pi * i / N) for i in range(N)]
ARC = [0.0]
for i in range(1, N + 1):
    p, q = OUT[i - 1], OUT[i % N]
    ARC.append(ARC[-1] + math.hypot(q[0] - p[0], q[1] - p[1]))
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
    return x, y, tx / l, ty / l  # point + CCW tangent (water on the left = local +Y)


def frame_at_angle(ang_deg):
    t = math.radians(ang_deg)
    x, y = shore_pt(t)
    x2, y2 = shore_pt(t + 1e-3)
    tx, ty = x2 - x, y2 - y
    l = math.hypot(tx, ty)
    return x, y, tx / l, ty / l


XS = [p[0] for p in OUT]
YS = [p[1] for p in OUT]
BBOX = (min(XS), max(XS), min(YS), max(YS))


def pass_shore():
    removed = clear("L01/Shore")
    # Old "swimming pool": box walls + placeholder dock.
    gone = []
    for lab in ["Shore_N", "Shore_S", "Shore_E", "Shore_W", "Dock_Deck", "Dock_Post_A", "Dock_Post_B"]:
        a = find(lab)
        if a:
            execute_tool(SC + "remove_from_scene", json.dumps({"actor": a}))
            gone.append(lab)
    # Water plane + lake bed cover the outline's bounding box plus a margin (hidden under the land beyond the shore).
    sx = (max(-BBOX[0], BBOX[1]) + WATER_MARGIN) * 2.0
    sy = (max(-BBOX[2], BBOX[3]) + WATER_MARGIN) * 2.0
    lake = find("LF_Lake")
    setp(lake, {"Size": {"X": sx * CM, "Y": sy * CM}})
    bed = find("LakeBed")
    execute_tool(AT + "set_actor_transform", json.dumps({"actor": bed, "xform": {"location": {"x": 0, "y": 0, "z": -450},
        "scale": {"x": sx * SCALE + 4, "y": sy * SCALE + 4, "z": 1}}}))
    # Straights every SHORE_STEP m of shoreline (overlapping), tangent-aligned, water on +Y.
    n = int(PERIM / SHORE_STEP) + 1
    for i in range(n):
        x, y, tx, ty = at_arc(i * PERIM / n)
        add("SM_LF_Shore_Straight", "Shore_%03d" % i, x, y, 0.0, math.degrees(math.atan2(ty, tx)), folder="L01/Shore")
    # Corners: CornerOut on the points' tips (land inside the arc), CornerIn at the back of the small bays (water inside).
    for k, ang in enumerate(list(POINTS.keys())):
        x, y, tx, ty = frame_at_angle(ang)
        nx, ny = -ty, tx  # into the water
        cx, cy = x - 4 * nx / SCALE, y - 4 * ny / SCALE
        add("SM_LF_Shore_CornerOut", "Shore_CornerOut_%d" % k, cx, cy, 0.0, math.degrees(math.atan2(ny, nx)) + 45.0, folder="L01/Shore")
    for k, ang in enumerate(list(SMALL_BAYS.keys())):
        x, y, tx, ty = frame_at_angle(ang)
        nx, ny = -ty, tx
        cx, cy = x + 4 * nx / SCALE, y + 4 * ny / SCALE
        add("SM_LF_Shore_CornerIn", "Shore_CornerIn_%d" % k, cx, cy, 0.0, math.degrees(math.atan2(-ny, -nx)) + 45.0, folder="L01/Shore")
    return {"removed_prev": removed, "removed_pool": gone, "perimeter_m": round(PERIM, 1), "straights": n,
            "outline_bbox_m": [round(v, 1) for v in BBOX], "water_size_m": [round(sx, 1), round(sy, 1)]}


def pass_beyond():
    removed = clear("L01/Beyond")
    rng = Rng(20261114)
    F = "L01/Beyond"
    # Ground: 18 m band tiles along the shore (rows 10 / 24 m inland), 40 m grid tiles (20 m where a 40 m tile would touch the
    # band) fully beyond shore + 4 m out to 150 m, then an 80 m ring out to 310 m. Edges at local Z=0, placed just under the
    # shore crest (0.42-0.6 m) so the water plane (max wave 12.5 cm) never shows through.
    tiles = 0
    for row, off in enumerate([10.0, 24.0]):
        n = int((PERIM + 2 * math.pi * off) / 14.0)
        for i in range(n):
            x, y, tx, ty = at_arc(i * PERIM / n)
            nx, ny = -ty, tx
            add(rng.pick(["SM_LF_GroundPatch_01", "SM_LF_GroundPatch_02"]), "Ground_B%d_%03d" % (row, i), x - nx * off, y - ny * off,
                0.42 - 0.01 * row, math.degrees(math.atan2(ty, tx)) + rng.pick([0, 90, 180, 270]), (0.45 * SCALE, 0.45 * SCALE, 0.5), F + "/Ground")
            tiles += 1

    def beyond(cx, cy, half, margin):
        pts = [(cx + dx * half, cy + dy * half) for dx in (-1, 0, 1) for dy in (-1, 0, 1)]
        return all(inland(px, py) > margin for px, py in pts)

    ext = 160.0
    g = -ext
    gi = 0
    while g < ext:
        h = -ext
        while h < ext:
            cx, cy = g + 20.0, h + 20.0
            if beyond(cx, cy, 20.0, 4.0):
                add(rng.pick(["SM_LF_GroundPatch_01", "SM_LF_GroundPatch_02"]), "Ground_G%03d" % gi, cx, cy, 0.38, rng.pick([0, 90, 180, 270]),
                    (SCALE, SCALE, 1.0), F + "/Ground"); gi += 1; tiles += 1
            else:
                for qx in (-10.0, 10.0):
                    for qy in (-10.0, 10.0):
                        if beyond(cx + qx, cy + qy, 10.0, 4.0):
                            add(rng.pick(["SM_LF_GroundPatch_01", "SM_LF_GroundPatch_02"]), "Ground_G%03d" % gi, cx + qx, cy + qy, 0.39,
                                rng.pick([0, 90, 180, 270]), (0.5 * SCALE, 0.5 * SCALE, 0.7), F + "/Ground"); gi += 1; tiles += 1
            h += 40.0
        g += 40.0
    for gx in range(-4, 4):
        for gy in range(-4, 4):
            cx, cy = gx * 80.0 + 40.0, gy * 80.0 + 40.0
            if max(abs(cx), abs(cy)) - 40.0 >= ext - 1.0:  # outside the 40 m grid
                add(rng.pick(["SM_LF_GroundPatch_01", "SM_LF_GroundPatch_02"]), "Ground_F%03d" % gi, cx, cy, 0.36, rng.pick([0, 90, 180, 270]),
                    (2.0 * SCALE, 2.0 * SCALE, 1.5), F + "/Ground"); gi += 1; tiles += 1
    # Forest: single trees near the shore (8-30 m inland, sparse), clusters further back (25-110 m, denser with distance).
    trees = 0
    for i in range(70):
        ang = rng.r(0, 360)
        t = math.radians(ang)
        r = shore_r(t) + rng.r(8.0, 30.0)
        if any(abs(adiff(ang, d)) < 9 for d in DOCK_ANG.values()):
            continue
        x, y = r * math.cos(t), r * math.sin(t)
        add(rng.pick(["SM_LF_Tree_Pine", "SM_LF_Tree_Pine", "SM_LF_Tree_Round"]), "Tree_%03d" % i, x, y, 0.4, rng.r(0, 360),
            (rng.r(0.8, 1.25),) * 3, F + "/Forest"); trees += 1
    for i in range(120):
        u = rng.f() ** 0.6  # biased outward: denser further back
        ang = rng.r(0, 360)
        t = math.radians(ang)
        r = shore_r(t) + 25.0 + u * 85.0
        if any(abs(adiff(ang, d)) < 6 for d in DOCK_ANG.values()) and r < shore_r(t) + 35:
            continue
        x, y = r * math.cos(t), r * math.sin(t)
        s = rng.r(0.9, 1.4)
        add("SM_LF_TreeCluster_01", "TreeCluster_%03d" % i, x, y, 0.35, rng.r(0, 360), (s, s, s * rng.r(0.9, 1.2)), F + "/Forest"); trees += 1
    # Rock + bush clumps on the bank.
    clumps = 0
    for i in range(14):
        ang = rng.r(0, 360)
        if any(abs(adiff(ang, d)) < 7 for d in DOCK_ANG.values()):
            continue
        t = math.radians(ang)
        r0 = shore_r(t) + rng.r(4.0, 10.0)
        for j in range(int(rng.r(3, 6))):
            a2, r2 = ang + rng.r(-2.5, 2.5), r0 + rng.r(-2.0, 3.0)
            x, y = polar(a2, r2)
            m = rng.pick(["SM_LF_Rock_01", "SM_LF_Rock_02", "SM_LF_Rock_03", "SM_LF_Bush_01", "SM_LF_Bush_02", "SM_LF_Bush_01"])
            add(m, "Clump_%02d_%d" % (i, j), x, y, 0.35, rng.r(0, 360), (rng.r(0.7, 1.3),) * 3, F + "/Clumps")
        clumps += 1
    # Far hills: a broken ring 280-380 m out, facing the lake, Z x1.4, overlapping; the forest hides their feet.
    hills = 0
    for i in range(9):
        ang = i * 40.0 + rng.r(-10, 10)
        r = rng.r(280.0, 380.0)
        x, y = polar(ang, r)
        m = rng.pick(["SM_LF_Hill_Far_01", "SM_LF_Hill_Far_02", "SM_LF_Hill_Far_03"])
        add(m, "Hill_%d" % i, x, y, 0.0, ang + 90.0, (SCALE, SCALE, 1.4 * SCALE), F + "/Hills"); hills += 1
    return {"removed_prev": removed, "ground_tiles": tiles, "trees": trees, "clumps": clumps, "hills": hills}


def pass_gameplay():
    removed = clear("L01/Gameplay")
    rng = Rng(777)
    F = "L01/Gameplay"
    out = {"removed_prev": removed}
    docks = {}
    for team, ang in DOCK_ANG.items():
        # Dock along the radial line, +X toward the centre (flag tray at the water end), deck 0.5 m above the water.
        x, y = polar(ang, DOCK_R + 2.0)
        yaw = ang + 180.0
        add("SM_LF_Dock_Team", "Dock_%s" % team, x, y, 0.5, yaw, folder=F + "/Docks")
        t = math.radians(yaw)
        fx = x + (1.82 * math.cos(t) + 1.08 * math.sin(t)) / SCALE  # dock mesh is unscaled: true-metre offset
        fy = y + (1.82 * math.sin(t) - 1.08 * math.cos(t)) / SCALE
        add("SM_LF_TeamFlag_%s" % team, "Flag_%s" % team, fx, fy, 0.5 + 0.70, yaw, folder=F + "/Docks")
        docks[team] = (x, y)
        # Dressing by the dock: a log at the waterline, a bush and a fence section on the bank.
        sx, sy = polar(ang + 6.0, shore_r(math.radians(ang + 6.0)) + 1.5)
        add("SM_LF_Log", "DockLog_%s" % team, sx, sy, 0.05, ang + 75.0, folder=F + "/Dressing")
        bx, by = polar(ang - 7.0, shore_r(math.radians(ang - 7.0)) + 5.0)
        add("SM_LF_Bush_02", "DockBush_%s" % team, bx, by, 0.4, rng.r(0, 360), folder=F + "/Dressing")
        for k in range(2):
            ex, ey = polar(ang + (k - 0.5) * 9.0, shore_r(math.radians(ang)) + 6.5)
            add("SM_LF_Fence_Section", "DockFence_%s_%d" % (team, k), ex, ey, 0.45, ang + 90.0, folder=F + "/Dressing")
    # Spawns: each crew's boat 4 m off its dock's water end, bow to the centre.
    sp = {}
    for team, ang in DOCK_ANG.items():
        sp[team] = polar(ang, DOCK_R - 4.0) + (ang + 180.0,)
    for team, lab in [("Blue", "LF_Boat_Blue"), ("Green", "LF_Boat_Green"), ("Yellow", "LF_Boat_Yellow")]:
        x, y, yaw = sp[team]
        for l, z in [(lab, 0.0), (lab.replace("Boat", "Crew"), 80.0)]:
            a = find(l)
            execute_tool(AT + "set_actor_transform", json.dumps({"actor": a, "xform": {"location": {"x": x * CM, "y": y * CM, "z": z},
                "rotation": {"pitch": 0, "yaw": yaw, "roll": 0}}}))
    x, y, yaw = sp["Red"]
    ps = find("PlayerStart")
    execute_tool(AT + "set_actor_transform", json.dumps({"actor": ps, "xform": {"location": {"x": x * CM, "y": y * CM, "z": 40},
        "rotation": {"pitch": 0, "yaw": yaw, "roll": 0}}}))
    # Islands with dressing.
    for name, ang, r, mesh, wr in ISLANDS:
        x, y = polar(ang, r)
        add(mesh, "Island_%s" % name, x, y, 0.0, rng.r(0, 360), folder=F + "/Islands")
        top = 0.7 if mesh.endswith("_M") else 0.45
        if name == "N":
            add("SM_LF_Tree_Pine", "Island_N_Pine", x - 1.2, y + 1.0, top - 0.1, 20, (1.1, 1.1, 1.1), F + "/Islands")
            add("SM_LF_Tree_Round", "Island_N_Tree", x + 1.8, y - 0.8, top - 0.15, 140, (0.8, 0.8, 0.8), F + "/Islands")
            add("SM_LF_Rock_02", "Island_N_Rock", x + 0.5, y + 2.6, top - 0.3, 60, folder=F + "/Islands")
        elif name == "E":
            add("SM_LF_Tree_Round", "Island_E_Tree", x, y, top - 0.1, 200, (0.75, 0.75, 0.75), F + "/Islands")
            add("SM_LF_Reeds", "Island_E_Reeds", x + 2.2, y + 1.6, 0.0, 30, folder=F + "/Islands")
        else:
            add("SM_LF_Rock_03", "Island_W_Rock", x + 0.3, y - 0.2, top - 0.35, 300, (0.8, 0.8, 0.9), F + "/Islands")
            add("SM_LF_Rock_01", "Island_W_Rock2", x - 1.2, y + 0.9, top - 0.2, 10, folder=F + "/Islands")
            add("SM_LF_Reeds", "Island_W_Reeds", x - 2.3, y - 1.4, 0.0, 120, folder=F + "/Islands")
    # Channels (island waterline edge -> shoreline), centre clearance.
    ch = {}
    for name, ang, r, mesh, wr in ISLANDS:
        ch[name] = round(shore_r(math.radians(ang)) - (r + wr), 1)
    out["channels_m"] = ch
    out["centre_clear_radius_m"] = round(min(r - wr for _, _, r, _, wr in ISLANDS), 1)
    # Reed beds (pass-through): two at channel mouths, beside two docks.
    reeds = [(74.0, 50.0, "SM_LF_ReedBed"), (12.0, 63.0, "SM_LF_ReedBed_v1"),
             (DOCK_ANG["Green"] + 8.0, None, "SM_LF_ReedBed"), (DOCK_ANG["Yellow"] - 8.0, None, "SM_LF_ReedBed_v1")]
    for k, (ang, r, mesh) in enumerate(reeds):
        if r is None:
            r = shore_r(math.radians(ang)) - 3.5
        x, y = polar(ang, r)
        add(mesh, "ReedBed_%d" % k, x, y, -0.15, rng.r(0, 360), (1.4, 1.4, 1.0), F + "/Reeds")
    # Lily pads, stumps (shallows, collision on), buoys marking the channel mouths.
    for k, ang in enumerate([60, 125, 200, 245, 300, 330]):
        x, y = polar(ang, shore_r(math.radians(ang)) - rng.r(3.0, 6.0))
        add("SM_LF_LilyPads", "LilyPads_%d" % k, x, y, 0.03, rng.r(0, 360), (1.2, 1.2, 1.0), F + "/Dressing")
    for k, ang in enumerate([100, 165, 225, 315]):
        x, y = polar(ang, shore_r(math.radians(ang)) - 3.0)
        add("SM_LF_Stump", "Stump_%d" % k, x, y, -0.05, rng.r(0, 360), folder=F + "/Dressing")
    for name, ang, r, mesh, wr in ISLANDS:
        for side in (-1, 1):
            a2 = ang + side * 12.0
            rr = (r + wr + shore_r(math.radians(ang))) * 0.5
            x, y = polar(a2, rr)
            add("SM_LF_Buoy", "Buoy_%s_%d" % (name, side), x, y, 0.0, rng.r(0, 360), folder=F + "/Dressing")
    # Shack on the Blue dock's shore, facing the water, with crates, a fence and a log.
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
    # Fish zones (editor-visible TargetPoints): centre + the N and E channels.
    zones = {"FishZone_Centre": (0.0, 0.0)}
    for isl in ISLANDS:
        if isl[0] in ("N", "E"):
            zones["FishZone_Channel%s" % isl[0]] = polar(isl[1], (isl[2] + isl[4] + shore_r(math.radians(isl[1]))) * 0.5)
    for lab, (x, y) in zones.items():
        add(None, lab, x, y, 0.5, cls="/Script/Engine.TargetPoint", folder=F + "/FishZones")
    out["fishzone_min_dock_dist_m"] = {k: round(min(math.hypot(x - dx, y - dy) for dx, dy in docks.values()), 1) for k, (x, y) in zones.items()}
    out["dock_centre_dist_m"] = {t: round(math.hypot(*polar(a, DOCK_R)), 1) for t, a in DOCK_ANG.items()}
    return out


def run():
    res = {"scale": SCALE}
    if PASS in ("shore", "all"):
        res["shore"] = pass_shore()
    if PASS in ("beyond", "all"):
        res["beyond"] = pass_beyond()
    if PASS in ("gameplay", "all"):
        res["gameplay"] = pass_gameplay()
    res["placed"] = STATS
    return res
