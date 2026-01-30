bl_info = {
    "name": "GSE Exporter",
    "author": "GSEngine",
    "version": (1, 0, 0),
    "blender": (4, 0, 0),
    "location": "File > Export > GSE Export",
    "description": "Export skinned meshes, skeletons, and animations for GSEngine",
    "category": "Import-Export",
}

import bpy
import struct
import os
from mathutils import Matrix, Vector
from bpy_extras.io_utils import ExportHelper
from bpy.props import StringProperty, BoolProperty

GSKEL_MAGIC = b'GSKL'
GCLIP_MAGIC = b'GCLP'
GSMDL_MAGIC = b'GSMD'
VERSION = 1

# Coordinate system conversion: Blender (Z-up) to Engine (Y-up)
# Blender: X-right, Y-forward, Z-up
# Engine:  X-right, Y-up, Z-back (right-handed)
# Transform: new_x = old_x, new_y = old_z, new_z = -old_y
COORD_CONVERT = Matrix((
    (1,  0,  0, 0),
    (0,  0,  1, 0),
    (0, -1,  0, 0),
    (0,  0,  0, 1)
))
COORD_CONVERT_INV = COORD_CONVERT.inverted()

def convert_position(pos):
    """Convert a position from Blender coords to engine coords."""
    return (pos[0], pos[2], -pos[1])

def convert_direction(dir):
    """Convert a direction/normal from Blender coords to engine coords."""
    return (dir[0], dir[2], -dir[1])

def convert_matrix(mat):
    """Convert a transformation matrix from Blender coords to engine coords."""
    # For a transform M in Blender space, engine space is: C @ M @ C^(-1)
    return COORD_CONVERT @ mat @ COORD_CONVERT_INV

def write_string(f, s):
    encoded = s.encode('utf-8')
    f.write(struct.pack('<I', len(encoded)))
    f.write(encoded)

def write_mat4(f, mat):
    # Transpose the matrix: Blender uses row-vectors (translation in row 3),
    # but the engine uses column-vectors (translation in column 3)
    for row in range(4):
        for col in range(4):
            f.write(struct.pack('<f', mat[row][col]))  # Note: swapped row/col

def write_vec3(f, v):
    f.write(struct.pack('<fff', v[0], v[1], v[2]))

def write_vec2(f, v):
    f.write(struct.pack('<ff', v[0], v[1]))

def write_vec4(f, v):
    f.write(struct.pack('<ffff', v[0], v[1], v[2], v[3]))

def get_armature_and_meshes(context):
    """Returns armature and list of ALL mesh objects parented to it."""
    armature = None
    mesh_objects = []

    # Find armature from selection or scene
    for obj in context.selected_objects:
        if obj.type == 'ARMATURE':
            armature = obj
            break

    if armature is None:
        for obj in bpy.data.objects:
            if obj.type == 'ARMATURE':
                armature = obj
                break

    # Find ALL mesh objects parented to the armature
    if armature:
        for obj in bpy.data.objects:
            if obj.type == 'MESH' and obj.parent == armature:
                mesh_objects.append(obj)

    return armature, mesh_objects


def get_armature_and_mesh(context):
    """Legacy function - returns first mesh for compatibility."""
    armature, mesh_objects = get_armature_and_meshes(context)
    mesh_obj = mesh_objects[0] if mesh_objects else None
    return armature, mesh_obj

def build_bone_order(armature):
    bones = []
    bone_name_to_index = {}

    def add_bone_recursive(bone):
        if bone.name in bone_name_to_index:
            return
        bone_name_to_index[bone.name] = len(bones)
        bones.append(bone)
        for child in bone.children:
            add_bone_recursive(child)

    for bone in armature.data.bones:
        if bone.parent is None:
            add_bone_recursive(bone)

    return bones, bone_name_to_index

def compute_scale_correction(armature, mesh_obj):
    """
    Compute scale factor to match skeleton to mesh coordinate space.
    Compares actual geometry dimensions rather than relying on object scales.
    """
    if mesh_obj is None:
        return 1.0

    # Get mesh bounding box in world space - use maximum dimension for robustness
    mesh_bbox = [mesh_obj.matrix_world @ Vector(corner) for corner in mesh_obj.bound_box]
    mesh_dims = [
        max(v.x for v in mesh_bbox) - min(v.x for v in mesh_bbox),
        max(v.y for v in mesh_bbox) - min(v.y for v in mesh_bbox),
        max(v.z for v in mesh_bbox) - min(v.z for v in mesh_bbox),
    ]
    mesh_size = max(mesh_dims)  # Use largest dimension (typically height)

    # Get armature size from bone positions - use maximum dimension
    bone_positions = []
    for bone in armature.data.bones:
        head_world = armature.matrix_world @ bone.head_local
        tail_world = armature.matrix_world @ bone.tail_local
        bone_positions.extend([head_world, tail_world])

    if not bone_positions:
        return 1.0

    arm_dims = [
        max(v.x for v in bone_positions) - min(v.x for v in bone_positions),
        max(v.y for v in bone_positions) - min(v.y for v in bone_positions),
        max(v.z for v in bone_positions) - min(v.z for v in bone_positions),
    ]
    arm_size = max(arm_dims)  # Use largest dimension

    # Compute scale ratio (mesh_size / armature_size)
    if arm_size > 0.001:  # Avoid division by zero
        scale = mesh_size / arm_size
    else:
        scale = 1.0

    return scale

def build_bone_world_matrix(armature, bone):
    """
    Build a bone's world-space transform matrix with proper scaling.

    The bone's head position is transformed by armature.matrix_world to get
    correct world position (including scale). The rotation comes from the
    full transformed matrix, normalized to remove scale artifacts.
    """
    # Get bone head in world space (properly scaled by matrix_world)
    head_world = armature.matrix_world @ bone.head_local

    # Get the bone's world rotation from the full transform, normalized
    bone_world_full = armature.matrix_world @ bone.matrix_local
    world_rot = bone_world_full.to_3x3().normalized()

    # Build the final matrix with correct position and rotation
    result = world_rot.to_4x4()
    result.translation = head_world

    return result

def export_skeleton(filepath, armature, mesh_obj=None):
    bones, bone_name_to_index = build_bone_order(armature)

    with open(filepath, 'wb') as f:
        f.write(GSKEL_MAGIC)
        f.write(struct.pack('<I', VERSION))
        f.write(struct.pack('<I', len(bones)))

        for i, bone in enumerate(bones):
            write_string(f, bone.name)

            if bone.parent and bone.parent.name in bone_name_to_index:
                parent_index = bone_name_to_index[bone.parent.name]
            else:
                parent_index = 0xFFFF
            f.write(struct.pack('<H', parent_index))

            # Build bone world matrix with proper scaling
            bone_world = build_bone_world_matrix(armature, bone)

            # Local bind: transform from parent bone space to this bone space
            if bone.parent:
                parent_world = build_bone_world_matrix(armature, bone.parent)
                local_bind = parent_world.inverted() @ bone_world
            else:
                local_bind = bone_world

            # Convert to engine coordinate system:
            # For skeleton matrices, we need: C @ M @ C^(-1) for the rotation part
            # But for the translation, we just convert the position
            local_bind_converted = convert_matrix(local_bind)
            write_mat4(f, local_bind_converted)

            # Inverse bind: transforms world-space vertices to bone-local space
            # Since vertices are now in engine coords, inverse_bind must also work in engine coords
            inverse_bind = bone_world.inverted()
            inverse_bind_converted = convert_matrix(inverse_bind)
            write_mat4(f, inverse_bind_converted)

    return bones, bone_name_to_index

def build_pose_bone_world_matrix(armature, pose_bone):
    """
    Build a pose bone's world-space transform matrix with proper scaling.

    Similar to build_bone_world_matrix but for animated pose bones.
    The position is properly scaled, and rotation is normalized.
    """
    # Get pose bone position in world space (properly scaled)
    # pose_bone.matrix.translation is in armature local space
    head_world = armature.matrix_world @ pose_bone.matrix.translation

    # Get the bone's world rotation from the full transform, normalized
    bone_world_full = armature.matrix_world @ pose_bone.matrix
    world_rot = bone_world_full.to_3x3().normalized()

    # Build the final matrix with correct position and rotation
    result = world_rot.to_4x4()
    result.translation = head_world

    return result

def export_clip(filepath, armature, action, bone_name_to_index, mesh_obj=None):
    bones, _ = build_bone_order(armature)

    frame_start = int(action.frame_range[0])
    frame_end = int(action.frame_range[1])
    fps = bpy.context.scene.render.fps
    length = (frame_end - frame_start) / fps

    original_action = armature.animation_data.action if armature.animation_data else None
    if armature.animation_data is None:
        armature.animation_data_create()
    armature.animation_data.action = action

    tracks = {}

    for frame in range(frame_start, frame_end + 1):
        bpy.context.scene.frame_set(frame)
        time_seconds = (frame - frame_start) / fps

        for pose_bone in armature.pose.bones:
            if pose_bone.name not in bone_name_to_index:
                continue

            joint_index = bone_name_to_index[pose_bone.name]

            # Build world matrix with proper scaling
            bone_world = build_pose_bone_world_matrix(armature, pose_bone)

            # Local transform: relative to parent (or world for root bones)
            if pose_bone.parent:
                parent_world = build_pose_bone_world_matrix(armature, pose_bone.parent)
                local_transform = parent_world.inverted() @ bone_world
            else:
                local_transform = bone_world

            # Convert to engine coordinate system
            local_transform_converted = convert_matrix(local_transform)

            if joint_index not in tracks:
                tracks[joint_index] = []

            tracks[joint_index].append((time_seconds, local_transform_converted.copy()))

    if original_action:
        armature.animation_data.action = original_action

    with open(filepath, 'wb') as f:
        f.write(GCLIP_MAGIC)
        f.write(struct.pack('<I', VERSION))

        write_string(f, action.name)
        f.write(struct.pack('<f', length))
        f.write(struct.pack('<B', 1))

        f.write(struct.pack('<I', len(tracks)))

        for joint_index, keys in tracks.items():
            f.write(struct.pack('<H', joint_index))
            f.write(struct.pack('<I', len(keys)))

            for time_seconds, local_transform in keys:
                f.write(struct.pack('<f', time_seconds))
                write_mat4(f, local_transform)

    return True

def get_material_textures(material):
    textures = {
        'diffuse': None,
        'normal': None,
        'specular': None
    }

    if material is None or not material.use_nodes:
        return textures

    for node in material.node_tree.nodes:
        if node.type == 'BSDF_PRINCIPLED':
            base_color_input = node.inputs.get('Base Color')
            if base_color_input and base_color_input.is_linked:
                linked_node = base_color_input.links[0].from_node
                if linked_node.type == 'TEX_IMAGE' and linked_node.image:
                    textures['diffuse'] = linked_node.image

            normal_input = node.inputs.get('Normal')
            if normal_input and normal_input.is_linked:
                linked_node = normal_input.links[0].from_node
                if linked_node.type == 'NORMAL_MAP':
                    color_input = linked_node.inputs.get('Color')
                    if color_input and color_input.is_linked:
                        tex_node = color_input.links[0].from_node
                        if tex_node.type == 'TEX_IMAGE' and tex_node.image:
                            textures['normal'] = tex_node.image
                elif linked_node.type == 'TEX_IMAGE' and linked_node.image:
                    textures['normal'] = linked_node.image

            specular_input = node.inputs.get('Specular IOR Level') or node.inputs.get('Specular')
            if specular_input and specular_input.is_linked:
                linked_node = specular_input.links[0].from_node
                if linked_node.type == 'TEX_IMAGE' and linked_node.image:
                    textures['specular'] = linked_node.image

    return textures

def export_texture(image, textures_dir):
    if image is None:
        return None

    safe_name = bpy.path.clean_name(image.name)
    if not safe_name.lower().endswith(('.png', '.jpg', '.jpeg')):
        safe_name += '.png'

    export_path = os.path.join(textures_dir, safe_name)

    original_path = image.filepath_raw
    original_format = image.file_format

    try:
        image.filepath_raw = export_path
        image.file_format = 'PNG' if safe_name.lower().endswith('.png') else 'JPEG'
        image.save()
    finally:
        image.filepath_raw = original_path
        image.file_format = original_format

    return safe_name

def export_materials_mtl(mesh_obj, resources_root, asset_name):
    textures_dir = os.path.join(resources_root, "Textures")
    skinned_models_dir = os.path.join(resources_root, "SkinnedModels")

    os.makedirs(textures_dir, exist_ok=True)

    mtl_path = os.path.join(skinned_models_dir, asset_name + ".mtl")

    exported_materials = []

    with open(mtl_path, 'w') as f:
        for slot in mesh_obj.material_slots:
            if slot.material is None:
                continue

            mat = slot.material
            mat_name = mat.name
            exported_materials.append(mat_name)

            textures = get_material_textures(mat)

            diffuse_file = None
            normal_file = None
            specular_file = None

            if textures['diffuse']:
                diffuse_file = export_texture(textures['diffuse'], textures_dir)
            if textures['normal']:
                normal_file = export_texture(textures['normal'], textures_dir)
            if textures['specular']:
                specular_file = export_texture(textures['specular'], textures_dir)

            f.write(f"newmtl {mat_name}\n")
            f.write("Ka 1.0 1.0 1.0\n")
            f.write("Kd 1.0 1.0 1.0\n")
            f.write("Ks 0.0 0.0 0.0\n")
            f.write("Ns 0.0\n")
            f.write("d 1.0\n")
            f.write("illum 2\n")

            if diffuse_file:
                f.write(f"map_Kd {diffuse_file}\n")
            if normal_file:
                f.write(f"map_Bump {normal_file}\n")
            if specular_file:
                f.write(f"map_Ks {specular_file}\n")

            f.write("\n")

    return mtl_path, exported_materials

def export_skinned_model(filepath, mesh_objects, armature, bone_name_to_index, resources_root, asset_name):
    """Export skinned model from one or multiple mesh objects."""
    # Handle both single mesh and list of meshes for backwards compatibility
    if not isinstance(mesh_objects, list):
        mesh_objects = [mesh_objects]

    # Use first mesh for materials (could be improved to merge materials)
    mtl_path, exported_materials = export_materials_mtl(mesh_objects[0], resources_root, asset_name)

    # CRITICAL: Set armature to REST pose before exporting mesh vertices.
    # The Armature modifier deforms the mesh based on current pose, but our
    # inverse_bind matrices are computed from REST pose. If we export vertices
    # in a different pose, skinning will collapse vertices to joint positions.
    original_pose_position = armature.data.pose_position
    armature.data.pose_position = 'REST'

    # Force update to apply the rest pose
    bpy.context.view_layer.update()

    depsgraph = bpy.context.evaluated_depsgraph_get()

    vertices = []
    indices = []
    vertex_map = {}

    for mesh_obj in mesh_objects:

        mesh_eval = mesh_obj.evaluated_get(depsgraph)
        mesh = mesh_eval.to_mesh()

        mesh.calc_loop_triangles()
        if not mesh.uv_layers:
            mesh.uv_layers.new()
        uv_layer = mesh.uv_layers.active.data

        # Build vertex group to bone index mapping for THIS mesh object
        vertex_groups = mesh_obj.vertex_groups
        group_name_to_bone_index = {}
        for vg in vertex_groups:
            if vg.name in bone_name_to_index:
                group_name_to_bone_index[vg.index] = bone_name_to_index[vg.name]

        for tri in mesh.loop_triangles:
            for loop_index in tri.loops:
                loop = mesh.loops[loop_index]
                vert_index = loop.vertex_index
                vert = mesh.vertices[vert_index]

                # Get position in Blender world space, then convert to engine coords
                pos_blender = mesh_obj.matrix_world @ vert.co
                pos = convert_position(pos_blender)

                # Get normal in Blender world space, then convert to engine coords
                if tri.use_smooth:
                    normal_blender = (mesh_obj.matrix_world.to_3x3() @ vert.normal).normalized()
                else:
                    normal_blender = (mesh_obj.matrix_world.to_3x3() @ tri.normal).normalized()
                normal = convert_direction(normal_blender)

                uv = tuple(uv_layer[loop_index].uv)
                uv = (uv[0], 1.0 - uv[1])

                bone_indices = [0, 0, 0, 0]
                bone_weights = [0.0, 0.0, 0.0, 0.0]

                weights = []
                for g in vert.groups:
                    if g.group in group_name_to_bone_index:
                        bone_idx = group_name_to_bone_index[g.group]
                        weights.append((bone_idx, g.weight))

                weights.sort(key=lambda x: x[1], reverse=True)
                weights = weights[:4]

                total_weight = sum(w[1] for w in weights)
                if total_weight > 0:
                    for i, (bone_idx, weight) in enumerate(weights):
                        bone_indices[i] = bone_idx
                        bone_weights[i] = weight / total_weight
                else:
                    bone_weights[0] = 1.0

                vertex_key = (pos, normal, uv, tuple(bone_indices), tuple(bone_weights))

                if vertex_key in vertex_map:
                    indices.append(vertex_map[vertex_key])
                else:
                    new_index = len(vertices)
                    vertex_map[vertex_key] = new_index
                    vertices.append({
                        'position': pos,
                        'normal': normal,
                        'uv': uv,
                        'bone_indices': bone_indices,
                        'bone_weights': bone_weights
                    })
                    indices.append(new_index)

        mesh_eval.to_mesh_clear()

    with open(filepath, 'wb') as f:
        f.write(GSMDL_MAGIC)
        f.write(struct.pack('<I', VERSION))

        f.write(struct.pack('<I', 1))

        material_name = "default"
        if mesh_obj.material_slots and mesh_obj.material_slots[0].material:
            material_name = mesh_obj.material_slots[0].material.name
        write_string(f, material_name)

        f.write(struct.pack('<I', len(vertices)))

        for v in vertices:
            write_vec3(f, v['position'])
            write_vec3(f, v['normal'])
            write_vec2(f, v['uv'])
            f.write(struct.pack('<IIII', *v['bone_indices']))
            write_vec4(f, v['bone_weights'])

        f.write(struct.pack('<I', len(indices)))
        for idx in indices:
            f.write(struct.pack('<I', idx))

    # Restore the original pose position
    armature.data.pose_position = original_pose_position
    bpy.context.view_layer.update()

    return True, mtl_path

class GSE_OT_Export(bpy.types.Operator, ExportHelper):
    bl_idname = "export_scene.gse"
    bl_label = "Export GSE"
    bl_options = {'PRESET'}

    filename_ext = ""

    filepath: StringProperty(
        name="Engine Resources",
        description="Select the engine's Resources folder",
        subtype='DIR_PATH',
    )

    filter_glob: StringProperty(
        default="*",
        options={'HIDDEN'},
    )

    asset_name: StringProperty(
        name="Asset Name",
        description="Name for the exported asset (skeleton and mesh filenames)",
        default="character",
    )

    export_skeleton: BoolProperty(
        name="Export Skeleton",
        description="Export skeleton (.gskel)",
        default=True,
    )

    export_animations: BoolProperty(
        name="Export Animations",
        description="Export all animation clips (.gclip)",
        default=True,
    )

    export_mesh: BoolProperty(
        name="Export Skinned Mesh",
        description="Export skinned mesh (.gsmdl)",
        default=True,
    )

    def execute(self, context):
        armature, mesh_objects = get_armature_and_meshes(context)

        if armature is None:
            self.report({'ERROR'}, "No armature found")
            return {'CANCELLED'}

        resources_root = os.path.dirname(self.filepath) if os.path.isfile(self.filepath) else self.filepath
        if not os.path.isdir(resources_root):
            self.report({'ERROR'}, f"Invalid directory: {resources_root}")
            return {'CANCELLED'}

        skeletons_dir = os.path.join(resources_root, "Skeletons")
        clips_dir = os.path.join(resources_root, "Clips")
        skinned_models_dir = os.path.join(resources_root, "SkinnedModels")

        os.makedirs(skeletons_dir, exist_ok=True)
        os.makedirs(clips_dir, exist_ok=True)
        os.makedirs(skinned_models_dir, exist_ok=True)

        bone_name_to_index = {}

        # Use first mesh for skeleton export (for compatibility)
        first_mesh = mesh_objects[0] if mesh_objects else None

        if self.export_skeleton:
            skel_path = os.path.join(skeletons_dir, self.asset_name + ".gskel")
            _, bone_name_to_index = export_skeleton(skel_path, armature, first_mesh)
            self.report({'INFO'}, f"Exported skeleton: {skel_path}")
        else:
            _, bone_name_to_index = build_bone_order(armature)

        if self.export_animations:
            for action in bpy.data.actions:
                clip_path = os.path.join(clips_dir, action.name + ".gclip")
                export_clip(clip_path, armature, action, bone_name_to_index, first_mesh)
                self.report({'INFO'}, f"Exported clip: {clip_path}")

        if self.export_mesh:
            if not mesh_objects:
                self.report({'WARNING'}, "No mesh found for skinned export")
            else:
                mesh_path = os.path.join(skinned_models_dir, self.asset_name + ".gsmdl")
                # Pass ALL mesh objects to combine them into one exported mesh
                success, mtl_path = export_skinned_model(mesh_path, mesh_objects, armature, bone_name_to_index, resources_root, self.asset_name)
                self.report({'INFO'}, f"Exported skinned mesh: {mesh_path} ({len(mesh_objects)} mesh objects combined)")
                self.report({'INFO'}, f"Exported materials: {mtl_path}")

        return {'FINISHED'}

    def invoke(self, context, event):
        context.window_manager.fileselect_add(self)
        return {'RUNNING_MODAL'}

    def draw(self, context):
        layout = self.layout
        layout.prop(self, "asset_name")
        layout.separator()
        layout.prop(self, "export_skeleton")
        layout.prop(self, "export_animations")
        layout.prop(self, "export_mesh")

def menu_func_export(self, context):
    self.layout.operator(GSE_OT_Export.bl_idname, text="GSE Export (.gskel/.gclip/.gsmdl)")

def register():
    bpy.utils.register_class(GSE_OT_Export)
    bpy.types.TOPBAR_MT_file_export.append(menu_func_export)

def unregister():
    bpy.utils.unregister_class(GSE_OT_Export)
    bpy.types.TOPBAR_MT_file_export.remove(menu_func_export)

if __name__ == "__main__":
    register()
