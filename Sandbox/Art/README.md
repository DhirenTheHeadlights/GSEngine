# Art

Authoring output that engine tools consume: skinned meshes and skeletons
exported from a DCC tool, before they are turned into engine assets.

Nothing here is loaded at runtime. This directory sits outside Assets/ on
purpose -- the asset compiler scans Assets/ recursively, so an intermediate
file left in there is scanned, fails its version check, and reports nothing.

    Art/  ->  Tools/RigDerive  ->  Assets/
