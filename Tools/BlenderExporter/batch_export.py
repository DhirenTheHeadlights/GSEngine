"""Headless batch export of a Mixamo pack into engine assets.

Run under Blender:

    blender --background --python Tools/BlenderExporter/batch_export.py -- \
        --pack "<pack dir>" --character "X Bot.fbx" --project Sandbox

The character FBX supplies the armature, skin and textures; every other FBX in
the pack contributes its action as a clip. The skeleton and skinned mesh land in
the art directory as input for Tools/RigDerive; only clips and textures are
written into the scanned asset tree. Clips bind to the rig by bone name,
so the animation files only need matching bone names, not a matching joint
order.
"""

import argparse
import sys
from pathlib import Path

import bpy

sys.path.insert(0, str(Path(__file__).resolve().parent))

import exporter


def parse_args():
    argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
    ap = argparse.ArgumentParser()
    ap.add_argument('--pack', type=Path, required=True)
    ap.add_argument('--character', type=str, default=None,
                    help='omit to export clips only, leaving the existing rig untouched')
    ap.add_argument('--project', type=Path, default=Path('Sandbox'),
                    help='project root; art and asset directories come from its .gseproj')
    ap.add_argument('--out', type=Path, default=None,
                    help='engine-consumable assets (clips, textures)')
    ap.add_argument('--art', type=Path, default=None,
                    help='tool inputs (skeleton, skinned mesh) kept outside the scanned tree')
    return ap.parse_args(argv)


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


def reset_scene():
    bpy.ops.wm.read_factory_settings(use_empty=True)


def import_fbx(path):
    bpy.ops.import_scene.fbx(filepath=str(path))


def first_armature():
    for obj in bpy.context.scene.objects:
        if obj.type == 'ARMATURE':
            return obj
    return None


def meshes_for(armature):
    return [obj for obj in bpy.context.scene.objects
            if obj.type == 'MESH' and obj.find_armature() == armature]


def sanitise(name):
    return name.replace(' ', '_').lower()


def main():
    args = parse_args()
    character_path = args.pack / args.character if args.character else None

    art_default, assets_default = project_dirs(args.project)
    args.art = args.art or art_default
    args.out = args.out or assets_default

    clip_dir = args.out / 'Clips'
    texture_dir = args.out / 'Textures'
    for d in (clip_dir, texture_dir, args.art):
        d.mkdir(parents=True, exist_ok=True)

    bone_name_to_index = {}
    if args.character is not None:
        reset_scene()
        import_fbx(character_path)
        armature = first_armature()
        if armature is None:
            print('FAILED: no armature in character file')
            return 1

        meshes = meshes_for(armature)
        if not meshes:
            print('FAILED: no skinned meshes in character file')
            return 1

        _, bone_name_to_index = exporter.build_bone_order(armature)
        stem = sanitise(Path(args.character).stem)

        exporter.export_skeleton(str(args.art / f'{stem}.gskel'), armature, meshes[0])
        exporter.export_skinned_model(
            str(args.art / f'{stem}.gsmdl'), meshes, armature, bone_name_to_index, str(texture_dir)
        )
        print(f'skeleton   {stem}.gskel ({len(bone_name_to_index)} bones)')
        print(f'model      {stem}.gsmdl ({len(meshes)} mesh(es))')

    clips = ([args.pack] if args.pack.is_file()
             else sorted(p for p in args.pack.glob('*.fbx') if p.name != args.character))
    for clip_path in clips:
        reset_scene()
        import_fbx(clip_path)

        clip_armature = first_armature()
        if clip_armature is None or not clip_armature.animation_data or not clip_armature.animation_data.action:
            print(f'skipped    {clip_path.name} (no action)')
            continue

        _, clip_bone_index = exporter.build_bone_order(clip_armature)
        missing = set(bone_name_to_index) - set(clip_bone_index)

        action = clip_armature.animation_data.action
        out_name = sanitise(clip_path.stem)
        exporter.export_clip(
            str(clip_dir / f'{out_name}.gclip'), clip_armature, action, clip_bone_index
        )
        note = f', {len(missing)} rig bones absent' if missing else ''
        print(f'clip       {out_name}.gclip ({len(clip_bone_index)} bones{note})')

    return 0


if __name__ == '__main__':
    sys.exit(main())
