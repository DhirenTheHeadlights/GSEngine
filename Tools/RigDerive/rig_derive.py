"""Derive a physics rig from an authored skinned model.

Reads a .gskel (joint hierarchy + bind matrices) and a v1 .gsmdl (skinned mesh)
and writes a v3 .gsmdl carrying the mesh plus a fitted rigid-body rig: one
collision shape per kept bone, a proxy capsule, and skin weights remapped onto
the kept bone set.

Runs offline with no Blender and no engine dependency.

Version note: .gsmdl v2 is already claimed by the exporter for the PBR material
block, so the rig format is v3.
"""

import argparse
import math
import struct
import sys
from pathlib import Path

GSKEL_MAGIC = b'GSKL'
GSMDL_MAGIC = b'GSMD'
GSMDL_RIG_VERSION = 3

NO_BONE = 0xFFFF

SHAPE_BOX = 0
SHAPE_SPHERE = 1
SHAPE_CAPSULE = 2
SHAPE_NAMES = {SHAPE_BOX: 'box', SHAPE_SPHERE: 'sphere', SHAPE_CAPSULE: 'capsule'}

# Body-frame basis expressed in bone-local coordinates, per dominant axis, chosen
# so the bone's long axis becomes the body's +Y. The engine's capsule_shape is
# Y-aligned and carries no axis of its own, and mixamo bone-local space runs the
# bone along Z, so without this no limb would ever fit a capsule. Each is a
# proper rotation (det +1); columns are the body axes in local coordinates.
AXIS_BASIS = {
    0: [(0.0, -1.0, 0.0), (1.0, 0.0, 0.0), (0.0, 0.0, 1.0)],
    1: [(1.0, 0.0, 0.0), (0.0, 1.0, 0.0), (0.0, 0.0, 1.0)],
    2: [(1.0, 0.0, 0.0), (0.0, 0.0, 1.0), (0.0, -1.0, 0.0)],
}


# ---------------------------------------------------------------- math

def mat4_identity():
    return [1.0, 0.0, 0.0, 0.0,
            0.0, 1.0, 0.0, 0.0,
            0.0, 0.0, 1.0, 0.0,
            0.0, 0.0, 0.0, 1.0]


def mat4_mul(a, b):
    out = [0.0] * 16
    for r in range(4):
        for c in range(4):
            out[r * 4 + c] = sum(a[r * 4 + k] * b[k * 4 + c] for k in range(4))
    return out


def mat4_transform_point(m, p):
    x, y, z = p
    return (
        m[0] * x + m[1] * y + m[2] * z + m[3],
        m[4] * x + m[5] * y + m[6] * z + m[7],
        m[8] * x + m[9] * y + m[10] * z + m[11],
    )


def mat4_affine_inverse(m):
    a, b, c = m[0], m[1], m[2]
    d, e, f = m[4], m[5], m[6]
    g, h, i = m[8], m[9], m[10]
    tx, ty, tz = m[3], m[7], m[11]

    det = a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g)
    if abs(det) < 1e-20:
        raise ValueError('singular matrix')
    inv_det = 1.0 / det

    r = [
        (e * i - f * h) * inv_det, (c * h - b * i) * inv_det, (b * f - c * e) * inv_det,
        (f * g - d * i) * inv_det, (a * i - c * g) * inv_det, (c * d - a * f) * inv_det,
        (d * h - e * g) * inv_det, (b * g - a * h) * inv_det, (a * e - b * d) * inv_det,
    ]

    return [
        r[0], r[1], r[2], -(r[0] * tx + r[1] * ty + r[2] * tz),
        r[3], r[4], r[5], -(r[3] * tx + r[4] * ty + r[5] * tz),
        r[6], r[7], r[8], -(r[6] * tx + r[7] * ty + r[8] * tz),
        0.0, 0.0, 0.0, 1.0,
    ]


def mat4_from_basis(basis, translation):
    c0, c1, c2 = basis
    return [
        c0[0], c1[0], c2[0], translation[0],
        c0[1], c1[1], c2[1], translation[1],
        c0[2], c1[2], c2[2], translation[2],
        0.0, 0.0, 0.0, 1.0,
    ]


def mat4_to_quat(m):
    """Rotation part -> quaternion (w, x, y, z), scale removed."""
    cols = [
        (m[0], m[4], m[8]),
        (m[1], m[5], m[9]),
        (m[2], m[6], m[10]),
    ]
    lens = [math.sqrt(sum(v * v for v in col)) or 1.0 for col in cols]
    r = [
        cols[0][0] / lens[0], cols[1][0] / lens[1], cols[2][0] / lens[2],
        cols[0][1] / lens[0], cols[1][1] / lens[1], cols[2][1] / lens[2],
        cols[0][2] / lens[0], cols[1][2] / lens[1], cols[2][2] / lens[2],
    ]

    trace = r[0] + r[4] + r[8]
    if trace > 0.0:
        s = math.sqrt(trace + 1.0) * 2.0
        return (0.25 * s, (r[7] - r[5]) / s, (r[2] - r[6]) / s, (r[3] - r[1]) / s)
    if r[0] > r[4] and r[0] > r[8]:
        s = math.sqrt(1.0 + r[0] - r[4] - r[8]) * 2.0
        return ((r[7] - r[5]) / s, 0.25 * s, (r[1] + r[3]) / s, (r[2] + r[6]) / s)
    if r[4] > r[8]:
        s = math.sqrt(1.0 + r[4] - r[0] - r[8]) * 2.0
        return ((r[2] - r[6]) / s, (r[1] + r[3]) / s, 0.25 * s, (r[5] + r[7]) / s)
    s = math.sqrt(1.0 + r[8] - r[0] - r[4]) * 2.0
    return ((r[3] - r[1]) / s, (r[2] + r[6]) / s, (r[5] + r[7]) / s, 0.25 * s)


def quat_to_mat4(q, t):
    w, x, y, z = q
    n = math.sqrt(w * w + x * x + y * y + z * z) or 1.0
    w, x, y, z = w / n, x / n, y / n, z / n
    return [
        1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w), t[0],
        2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w), t[1],
        2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y), t[2],
        0.0, 0.0, 0.0, 1.0,
    ]


# ---------------------------------------------------------------- parsing

class Reader:
    def __init__(self, data):
        self.d = data
        self.p = 0

    def u32(self):
        v, = struct.unpack_from('<I', self.d, self.p)
        self.p += 4
        return v

    def u16(self):
        v, = struct.unpack_from('<H', self.d, self.p)
        self.p += 2
        return v

    def f32(self, n=1):
        v = struct.unpack_from('<' + 'f' * n, self.d, self.p)
        self.p += 4 * n
        return v

    def raw(self, n):
        v = self.d[self.p:self.p + n]
        self.p += n
        return v

    def string(self):
        return self.raw(self.u32())

    def mat4(self):
        return list(self.f32(16))


def parse_gskel(path):
    r = Reader(path.read_bytes())
    if r.raw(4) != GSKEL_MAGIC:
        raise ValueError(f'{path}: not a .gskel')
    r.u32()
    count = r.u32()

    joints = []
    for _ in range(count):
        name = r.string().decode('utf-8', 'replace')
        parent = r.u16()
        r.mat4()
        inverse_bind = r.mat4()
        joints.append({
            'name': name,
            'parent': parent if parent != 0xFFFF else NO_BONE,
            'inverse_bind': inverse_bind,
        })
    return joints


def parse_gsmdl_source(path):
    """Read an exporter-written .gsmdl. v1 stores a bare material string; the
    PBR block only appears at v2."""
    r = Reader(path.read_bytes())
    if r.raw(4) != GSMDL_MAGIC:
        raise ValueError(f'{path}: not a .gsmdl')
    version = r.u32()
    if version not in (1, 2):
        raise ValueError(f'{path}: expected exporter v1 or v2, got v{version}')
    mesh_count = r.u32()

    meshes = []
    for _ in range(mesh_count):
        if version >= 2:
            material = {
                'base_color': r.f32(3),
                'roughness': r.f32(1)[0],
                'metallic': r.f32(1)[0],
                'albedo': r.string(),
                'normal': r.string(),
                'rm': r.string(),
            }
        else:
            material = {
                'base_color': (1.0, 1.0, 1.0),
                'roughness': 0.5,
                'metallic': 0.0,
                'albedo': r.string(),
                'normal': b'',
                'rm': b'',
            }
        vertex_count = r.u32()
        verts = []
        for _ in range(vertex_count):
            pos = r.f32(3)
            nrm = r.f32(3)
            uv = r.f32(2)
            idx = struct.unpack_from('<IIII', r.d, r.p)
            r.p += 16
            wts = r.f32(4)
            verts.append({'pos': pos, 'nrm': nrm, 'uv': uv, 'idx': idx, 'wts': wts})
        index_count = r.u32()
        indices = list(struct.unpack_from('<' + 'I' * index_count, r.d, r.p))
        r.p += 4 * index_count
        meshes.append({'material': material, 'verts': verts, 'indices': indices})

    if r.p != len(r.d):
        raise ValueError(f'{path}: trailing bytes ({r.p} of {len(r.d)} consumed)')
    return meshes


# ---------------------------------------------------------------- fitting

def dominant_bone(v):
    best_i, best_w = v['idx'][0], v['wts'][0]
    for i, w in zip(v['idx'], v['wts']):
        if w > best_w:
            best_i, best_w = i, w
    return best_i


def fit_shape(points, capsule_ratio, sphere_ratio):
    """Fit a shape to bone-local points.

    Returns (kind, params, center_local, basis) where basis is the body frame
    expressed in bone-local coordinates, oriented so the bone's long axis is +Y.
    """
    xs = [p[0] for p in points]
    ys = [p[1] for p in points]
    zs = [p[2] for p in points]
    lo = (min(xs), min(ys), min(zs))
    hi = (max(xs), max(ys), max(zs))
    center = tuple((lo[k] + hi[k]) * 0.5 for k in range(3))
    half = tuple(max((hi[k] - lo[k]) * 0.5, 1e-4) for k in range(3))

    axis = max(range(3), key=lambda k: half[k])
    basis = AXIS_BASIS[axis]
    # Body axes are a permutation of local axes, so extents just reorder.
    body_half = tuple(
        half[[i for i, v in enumerate(col) if abs(v) > 0.5][0]] for col in basis
    )

    cross = (body_half[0] + body_half[2]) * 0.5
    if body_half[1] > capsule_ratio * cross:
        radius = cross
        return SHAPE_CAPSULE, (radius, max(body_half[1] - radius, 1e-4), 0.0), center, basis

    if max(body_half) < sphere_ratio * min(body_half):
        return SHAPE_SPHERE, (sum(body_half) / 3.0, 0.0, 0.0), center, basis

    return SHAPE_BOX, body_half, center, basis


def shape_volume(kind, params):
    if kind == SHAPE_BOX:
        return 8.0 * params[0] * params[1] * params[2]
    if kind == SHAPE_SPHERE:
        return (4.0 / 3.0) * math.pi * params[0] ** 3
    r, hh = params[0], params[1]
    return 2.0 * math.pi * r * r * hh + (4.0 / 3.0) * math.pi * r ** 3


def shape_extent(kind, params):
    if kind == SHAPE_BOX:
        return 2.0 * max(params)
    if kind == SHAPE_SPHERE:
        return 2.0 * params[0]
    return 2.0 * (params[0] + params[1])


# ---------------------------------------------------------------- derivation

def select_kept(joints, extents, min_extent):
    """Prune leaf subtrees only, so the kept set stays a connected subtree.

    Removing an interior joint would leave its rotation needing to propagate to
    descendants, forcing FK over the full authored hierarchy alongside the
    reduced body set. Restricting removal to leaves keeps FK self-contained.

    Pruning is by fitted physical size, not vertex count: the mesh is dense at
    the hands, so a finger joint dominates more vertices than the upper spine.
    """
    children = {i: [] for i in range(len(joints))}
    for i, j in enumerate(joints):
        if j['parent'] != NO_BONE:
            children[j['parent']].append(i)

    depth = [0] * len(joints)
    for i, j in enumerate(joints):
        d, p = 0, j['parent']
        while p != NO_BONE:
            d += 1
            p = joints[p]['parent']
        depth[i] = d

    kept = set(range(len(joints)))
    for i in sorted(range(len(joints)), key=lambda k: -depth[k]):
        if joints[i]['parent'] == NO_BONE:
            continue
        if any(c in kept for c in children[i]):
            continue
        if extents.get(i, 0.0) < min_extent:
            kept.discard(i)
    return kept


def nearest_kept_ancestor(joints, kept, index):
    j = index
    while j != NO_BONE and j not in kept:
        j = joints[j]['parent']
    return j


def group_by_dominant(verts, key):
    groups = {}
    for v in verts:
        groups.setdefault(key(v), []).append(v['pos'])
    return groups


def derive(joints, meshes, args):
    verts = [v for m in meshes for v in m['verts']]

    # Pass 1: provisional fits on the authored skeleton, purely to get a physical
    # size per joint so pruning can be size-based rather than density-based.
    raw_groups = group_by_dominant(verts, dominant_bone)
    extents = {}
    for j, pts in raw_groups.items():
        if j >= len(joints) or len(pts) < args.min_fit_points:
            continue
        local = [mat4_transform_point(joints[j]['inverse_bind'], p) for p in pts]
        kind, params, _, _ = fit_shape(local, args.capsule_ratio, args.sphere_ratio)
        extents[j] = shape_extent(kind, params)

    kept = select_kept(joints, extents, args.min_extent)
    kept_sorted = sorted(kept)
    remap = {j: nearest_kept_ancestor(joints, kept, j) for j in range(len(joints))}
    slot_of = {j: s for s, j in enumerate(kept_sorted)}

    # Remap and merge weights onto the kept set. Merging can only reduce the
    # number of distinct influences, so four slots always suffice.
    overflow = 0
    for v in verts:
        merged = {}
        for bi, w in zip(v['idx'], v['wts']):
            if w <= 0.0:
                continue
            target = remap.get(bi, NO_BONE)
            if target == NO_BONE:
                continue
            merged[target] = merged.get(target, 0.0) + w
        if not merged:
            merged = {kept_sorted[0]: 1.0}
        if len(merged) > 4:
            overflow += 1
            merged = dict(sorted(merged.items(), key=lambda kv: -kv[1])[:4])
        total = sum(merged.values()) or 1.0
        pairs = [(slot_of[b], w / total) for b, w in merged.items()]
        pairs += [(0, 0.0)] * (4 - len(pairs))
        v['slots'] = [p[0] for p in pairs]
        v['weights'] = [p[1] for p in pairs]

    # Pass 2: final fits over the merged assignment.
    by_slot = group_by_dominant(
        verts, lambda v: v['slots'][max(range(4), key=lambda k: v['weights'][k])]
    )

    bones = []
    for slot, j in enumerate(kept_sorted):
        joint = joints[j]
        inv_bind = joint['inverse_bind']
        pos_list = by_slot.get(slot, [])
        pts = [mat4_transform_point(inv_bind, p) for p in pos_list]
        degenerate = len(pts) < args.min_fit_points
        if degenerate:
            pts = [(-0.02, -0.02, -0.02), (0.02, 0.02, 0.02)]

        kind, params, center, basis = fit_shape(pts, args.capsule_ratio, args.sphere_ratio)

        joint_bind_world = mat4_affine_inverse(inv_bind)
        body_offset = mat4_from_basis(basis, center)
        body_bind_world = mat4_mul(joint_bind_world, body_offset)

        volume = shape_volume(kind, params)
        bones.append({
            'name': joint['name'],
            'source_index': j,
            'parent_slot': slot_of.get(remap.get(joint['parent'], NO_BONE), NO_BONE)
                           if joint['parent'] != NO_BONE else NO_BONE,
            'joint_bind_world': joint_bind_world,
            'body_offset': body_offset,
            'body_bind_world': body_bind_world,
            'inverse_bind': mat4_affine_inverse(body_bind_world),
            'kind': kind,
            'params': params,
            'volume': volume,
            'vertex_count': len(pos_list),
            'degenerate': degenerate,
        })

    total_volume = sum(b['volume'] for b in bones) or 1.0
    density = args.target_mass / total_volume if args.target_mass else args.density
    for b in bones:
        b['mass'] = b['volume'] * density

    # Clips are authored in joint space against the source skeleton, so the rig
    # stores the joint hierarchy plus a joint->body offset rather than a
    # body-space hierarchy. Runtime FK is then:
    #
    #     joint_world[j] = joint_world[parent] * clip_local[j]
    #     body_world[j]  = joint_world[j] * body_offset[j]
    #
    # Baking clips into body space instead would make runtime FK marginally
    # simpler but would tie every clip to a particular rig derivation, so a new
    # animation could not be dropped in without re-deriving.
    #
    # Pruning removes leaf subtrees only, so a kept bone's parent is always kept
    # and parent_slot mirrors the joint hierarchy exactly.
    for b in bones:
        if b['parent_slot'] == NO_BONE:
            joint_local = b['joint_bind_world']
        else:
            parent_inv = mat4_affine_inverse(bones[b['parent_slot']]['joint_bind_world'])
            joint_local = mat4_mul(parent_inv, b['joint_bind_world'])
        b['joint_local_offset'] = (joint_local[3], joint_local[7], joint_local[11])
        b['joint_local_rotation'] = mat4_to_quat(joint_local)
        off = b['body_offset']
        b['body_offset_translation'] = (off[3], off[7], off[11])
        b['body_offset_rotation'] = mat4_to_quat(off)

    return {
        'bones': bones,
        'proxy': fit_proxy(verts),
        'dropped': [(j, joints[j]['name']) for j in range(len(joints)) if j not in kept],
        'overflow': overflow,
        'source_joints': len(joints),
        'density': density,
        'total_volume': total_volume,
    }


def fit_proxy(verts):
    """Upright capsule enclosing the character.

    Radius comes from depth (Z) rather than the horizontal AABB: the bind pose is
    a T-pose, so X spans the outstretched arms and would give a uselessly fat
    proxy for a character that stands with its arms down.
    """
    xs = [v['pos'][0] for v in verts]
    ys = [v['pos'][1] for v in verts]
    zs = [v['pos'][2] for v in verts]
    y_lo, y_hi = min(ys), max(ys)
    radius = max((max(zs) - min(zs)) * 0.5, 1e-3)
    half_height = max((y_hi - y_lo) * 0.5 - radius, 1e-3)
    center = ((min(xs) + max(xs)) * 0.5, (y_lo + y_hi) * 0.5, (min(zs) + max(zs)) * 0.5)
    return {
        'radius': radius,
        'half_height': half_height,
        'center': center,
        'total_height': y_hi - y_lo,
        'x_span': max(xs) - min(xs),
    }


# ---------------------------------------------------------------- output

def write_gsmdl_v3(path, meshes, rig):
    out = bytearray()

    def u32(v):
        out.extend(struct.pack('<I', v))

    def u16(v):
        out.extend(struct.pack('<H', v))

    def f32(*v):
        out.extend(struct.pack('<' + 'f' * len(v), *v))

    def blob(b):
        u32(len(b))
        out.extend(b)

    out.extend(GSMDL_MAGIC)
    u32(GSMDL_RIG_VERSION)

    bones = rig['bones']
    u32(len(bones))
    for b in bones:
        blob(b['name'].encode('utf-8'))
        u16(b['source_index'])
        u16(b['parent_slot'] if b['parent_slot'] != NO_BONE else 0xFFFF)
        f32(*b['joint_local_offset'])
        f32(*b['joint_local_rotation'])
        f32(*b['body_offset_translation'])
        f32(*b['body_offset_rotation'])
        f32(*b['inverse_bind'])
        u32(b['kind'])
        f32(*b['params'])
        f32(b['mass'])

    p = rig['proxy']
    f32(p['radius'], p['half_height'])
    f32(*p['center'])

    u32(len(meshes))
    for m in meshes:
        mat = m['material']
        f32(*mat['base_color'])
        f32(mat['roughness'])
        f32(mat['metallic'])
        blob(mat['albedo'])
        blob(mat['normal'])
        blob(mat['rm'])
        u32(len(m['verts']))
        for v in m['verts']:
            f32(*v['pos'])
            f32(*v['nrm'])
            f32(*v['uv'])
            out.extend(bytes(v['slots']))
            f32(*v['weights'])
        u32(len(m['indices']))
        out.extend(struct.pack('<' + 'I' * len(m['indices']), *m['indices']))

    path.write_bytes(bytes(out))
    return len(out)


def verify_gsmdl_v3(path):
    """Re-read the written file and check the invariants the runtime relies on.

    The load-bearing one is the FK check: a bone's stored local_offset and
    local_rotation, walked down the hierarchy, must reproduce the bind world
    transform implied by its inverse_bind. That is exactly what the runtime does
    to spawn bodies at the bind pose, so if it disagrees here it will disagree
    there.
    """
    r = Reader(path.read_bytes())
    problems = []

    if r.raw(4) != GSMDL_MAGIC:
        return ['bad magic']
    version = r.u32()
    if version != GSMDL_RIG_VERSION:
        return [f'expected v{GSMDL_RIG_VERSION}, got v{version}']

    bone_count = r.u32()
    bones = []
    for i in range(bone_count):
        name = r.string().decode('utf-8', 'replace')
        source_index = r.u16()
        parent = r.u16()
        joint_offset = r.f32(3)
        joint_rotation = r.f32(4)
        body_translation = r.f32(3)
        body_rotation = r.f32(4)
        inv_bind = list(r.f32(16))
        kind = r.u32()
        params = r.f32(3)
        mass = r.f32(1)[0]
        if parent != 0xFFFF and parent >= i:
            problems.append(f'{name}: parent {parent} is not before slot {i}')
        if kind not in SHAPE_NAMES:
            problems.append(f'{name}: bad shape kind {kind}')
        if not mass > 0.0:
            problems.append(f'{name}: non-positive mass {mass}')
        bones.append({'name': name, 'source_index': source_index, 'parent': parent,
                      'joint_offset': joint_offset, 'joint_rotation': joint_rotation,
                      'body_translation': body_translation, 'body_rotation': body_rotation,
                      'inv_bind': inv_bind})

    proxy_radius, proxy_half = r.f32(2)
    r.f32(3)
    if not (proxy_radius > 0.0 and proxy_half > 0.0):
        problems.append('proxy capsule has non-positive extents')

    worst_fk = 0.0
    worst_name = ''
    for b in bones:
        joint_local = quat_to_mat4(b['joint_rotation'], b['joint_offset'])
        joint_world = (joint_local if b['parent'] == 0xFFFF
                       else mat4_mul(bones[b['parent']]['joint_world'], joint_local))
        b['joint_world'] = joint_world
        body_world = mat4_mul(joint_world,
                              quat_to_mat4(b['body_rotation'], b['body_translation']))
        expected = mat4_affine_inverse(b['inv_bind'])
        err = max(abs(body_world[k] - expected[k]) for k in range(16))
        if err > worst_fk:
            worst_fk, worst_name = err, b['name']
    if worst_fk > 1e-3:
        problems.append(f'fk mismatch up to {worst_fk:.2e} at {worst_name}')

    mesh_count = r.u32()
    total_verts = 0
    for _ in range(mesh_count):
        r.f32(3)
        r.f32(1)
        r.f32(1)
        r.string()
        r.string()
        r.string()
        vertex_count = r.u32()
        total_verts += vertex_count
        for _ in range(vertex_count):
            r.f32(3)
            r.f32(3)
            r.f32(2)
            slots = r.raw(4)
            weights = r.f32(4)
            if max(slots) >= bone_count:
                problems.append(f'vertex slot {max(slots)} >= bone count {bone_count}')
            if abs(sum(weights) - 1.0) > 1e-3:
                problems.append(f'vertex weights sum to {sum(weights):.4f}')
        index_count = r.u32()
        r.raw(4 * index_count)

    if r.p != len(r.d):
        problems.append(f'trailing bytes: consumed {r.p} of {len(r.d)}')

    seen = set()
    problems = [p for p in problems if not (p in seen or seen.add(p))]
    return problems, worst_fk, bone_count, total_verts


def report(rig, meshes, args, out_path, out_size):
    bones = rig['bones']
    verts = [v for m in meshes for v in m['verts']]
    lines = []
    add = lines.append

    add(f'source joints      {rig["source_joints"]}')
    add(f'kept bones         {len(bones)}')
    add(f'dropped            {len(rig["dropped"])}')
    add(f'vertices           {len(verts)}')
    add(f'triangles          {sum(len(m["indices"]) for m in meshes) // 3}')
    add('')

    add(f'{"slot":>4}  {"bone":<26} {"par":>4} {"shape":<8} {"dims (m)":<24} {"kg":>7} {"verts":>6}')
    for s, b in enumerate(bones):
        if b['kind'] == SHAPE_BOX:
            dims = f'{b["params"][0]*2:.3f} x {b["params"][1]*2:.3f} x {b["params"][2]*2:.3f}'
        elif b['kind'] == SHAPE_SPHERE:
            dims = f'r {b["params"][0]:.3f}'
        else:
            dims = f'r {b["params"][0]:.3f}  len {2*(b["params"][0]+b["params"][1]):.3f}'
        par = '-' if b['parent_slot'] == NO_BONE else str(b['parent_slot'])
        flag = ' *' if b['degenerate'] else ''
        add(f'{s:>4}  {b["name"][:26]:<26} {par:>4} {SHAPE_NAMES[b["kind"]]:<8} '
            f'{dims:<24} {b["mass"]:>7.2f} {b["vertex_count"]:>6}{flag}')

    shape_mix = {}
    for b in bones:
        shape_mix[SHAPE_NAMES[b['kind']]] = shape_mix.get(SHAPE_NAMES[b['kind']], 0) + 1

    add('')
    add(f'shapes             ' + ', '.join(f'{v} {k}' for k, v in sorted(shape_mix.items())))
    add(f'total mass         {sum(b["mass"] for b in bones):.1f} kg '
        f'(density {rig["density"]:.0f} kg/m^3, volume {rig["total_volume"]*1000:.1f} L)')

    p = rig['proxy']
    add(f'proxy capsule      r {p["radius"]:.3f}  half_height {p["half_height"]:.3f}  '
        f'total {2*(p["radius"]+p["half_height"]):.3f} m')
    add(f'character height   {p["total_height"]:.3f} m   t-pose arm span {p["x_span"]:.3f} m')

    bad = sum(1 for v in verts if abs(sum(v['weights']) - 1.0) > 1e-3)
    add(f'weight sums != 1   {bad}')
    add(f'influence overflow {rig["overflow"]}')
    degenerate = [b['name'] for b in bones if b['degenerate']]
    if degenerate:
        add(f'degenerate fits    {len(degenerate)} (*): {", ".join(degenerate)}')

    add('')
    if rig['dropped']:
        names = sorted(n for _, n in rig['dropped'])
        add(f'dropped joints ({len(names)}, folded into nearest kept ancestor):')
        for i in range(0, len(names), 4):
            add('  ' + '  '.join(f'{n[:26]:<26}' for n in names[i:i + 4]))
        add('')

    add(f'wrote {out_path}' + ('' if out_size == 0 else f'  ({out_size / 1024:.0f} KB)'))
    return '\n'.join(lines)


def project_dirs(project_root):
    """Resolve a project's art and assets directories from its .gseproj.

    The leaf names are declared per project, so tools must not assume them.
    """
    leaves = {'art': 'Art', 'assets': 'Assets'}
    manifests = sorted(project_root.glob('*.gseproj'))
    if manifests:
        section = None
        for raw in manifests[0].read_text(encoding='utf-8', errors='replace').splitlines():
            line = raw.strip()
            if line.startswith('['):
                section = line.strip('[]').strip()
            elif section == 'project' and '=' in line:
                key, _, value = line.partition('=')
                key = key.strip()
                if key in leaves:
                    leaves[key] = value.strip()
    return project_root / leaves['art'], project_root / leaves['assets']


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--project', type=Path, default=Path('Sandbox'),
                    help='project root; art and asset directories come from its .gseproj')
    ap.add_argument('--name', type=str, default='x_bot',
                    help='stem of the .gskel / .gsmdl pair inside the art directory')
    ap.add_argument('--skeleton', type=Path, default=None)
    ap.add_argument('--model', type=Path, default=None)
    ap.add_argument('--out', type=Path, default=None,
                    help='engine-consumable output directory or file')
    ap.add_argument('--min-extent', type=float, default=0.08,
                    help='prune a leaf bone whose fitted longest dimension is below this (m)')
    ap.add_argument('--min-fit-points', type=int, default=8,
                    help='below this a bone gets a placeholder shape and is flagged')
    ap.add_argument('--capsule-ratio', type=float, default=1.5,
                    help='long half-extent must exceed this multiple of the cross extent')
    ap.add_argument('--sphere-ratio', type=float, default=1.3,
                    help='max/min half-extent below this is treated as isotropic')
    ap.add_argument('--density', type=float, default=1000.0, help='kg/m^3')
    ap.add_argument('--target-mass', type=float, default=None,
                    help='solve density so the rig totals this mass (kg); overrides --density')
    ap.add_argument('--dry-run', action='store_true', help='report only, write nothing')
    args = ap.parse_args()

    art_dir, assets_dir = project_dirs(args.project)
    args.skeleton = args.skeleton or art_dir / f'{args.name}.gskel'
    args.model = args.model or art_dir / f'{args.name}.gsmdl'
    args.out = args.out or assets_dir / 'SkinnedModels'

    joints = parse_gskel(args.skeleton)
    meshes = parse_gsmdl_source(args.model)
    rig = derive(joints, meshes, args)

    out_path = args.out / (args.model.stem + '.v3.gsmdl') if args.out.is_dir() else args.out
    size = 0 if args.dry_run else write_gsmdl_v3(out_path, meshes, rig)
    print(report(rig, meshes, args, 'nothing (dry run)' if args.dry_run else out_path, size))

    if args.dry_run:
        return 0

    problems, worst_fk, bone_count, vert_count = verify_gsmdl_v3(out_path)
    print(f'verify             {bone_count} bones, {vert_count} vertices re-read; '
          f'worst fk error {worst_fk:.2e} m')
    if problems:
        print('verify FAILED:')
        for p in problems:
            print(f'  {p}')
        return 1
    print('verify             ok')
    return 0


if __name__ == '__main__':
    sys.exit(main())
