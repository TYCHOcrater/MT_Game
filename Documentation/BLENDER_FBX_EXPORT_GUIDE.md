# Blender → Unreal FBX Export Guide

For character artists exporting characters to the MultiplayerTest UE 5.7 project.

If you follow this guide your FBX will import cleanly and animate correctly. Skipping any step usually causes silent breakage that's painful to debug after the fact (we've burned a full day on this — please read carefully).

---

## TL;DR — Required export settings

In Blender's FBX exporter, **Transform** section:

| Setting | Value |
|---|---|
| **Scale** | `1.00` |
| **Apply Scalings** | `FBX Units Scale` |
| **Forward** | `-Y Forward` |
| **Up** | `Z Up` |
| **Apply Unit** | ✓ ON |
| **Use Space Transform** | ✓ ON |

**Armature** section:

| Setting | Value |
|---|---|
| **Add Leaf Bones** | ✗ OFF |
| **Primary Bone Axis** | `Y Axis` |
| **Secondary Bone Axis** | `X Axis` |

**Geometry** section:

| Setting | Value |
|---|---|
| **Smoothing** | `Face` (or `Edge`) — **NOT** "Normals Only" |
| **Apply Modifiers** | ✓ ON |

**Bake Animation** section:

| Setting | Value |
|---|---|
| **Bake Animation** | ✗ OFF |

(We retarget animations from a shared library inside Unreal — don't bundle anims with the mesh.)

---

## Self-check before sending the FBX

Before you hand off the file, verify:

1. **Mesh height** — your character should be roughly 1.7–1.9 meters tall in Blender (matching real human scale). Not 0.017m, not 170m.
2. **Bone names** — should match the standard humanoid rig (`pelvis`, `spine_01..05`, `neck_01`, `head`, `clavicle_l/r`, `upperarm_l/r`, `lowerarm_l/r`, `hand_l/r`, `thigh_l/r`, `calf_l/r`, `foot_l/r`).
3. **Apply All Transforms before export** — in Object Mode, select the armature and mesh, press `Ctrl+A` → Apply → All Transforms. This bakes any object-level scale/rotation into the data so it doesn't end up as a "bone scale" in the FBX.
4. **Bind pose** — character should be in a clean A-pose or T-pose (matching whatever was agreed for the project; A-pose is preferred for the Unreal mannequin family).

---

## What goes wrong if you skip the settings

We hit each of these on the Desert character before writing this guide:

- **Bake Animation = ON** → Unreal imports a pile of junk AnimSequence assets at non-snap framerates (e.g., 2.933s @ 24fps).
- **Add Leaf Bones = ON** → adds `*_end` bones the project doesn't use, breaks IK Retargeter chain mapping.
- **Wrong Scale + Apply Scalings combo** → mesh imports at 1/100th size, OR (worse) the unit-conversion factor gets baked into the **root bone's Scale** as something like `(63.1, 63.1, 63.1)` instead of `(1, 1, 1)`. This silently breaks the IK Retargeter — characters look correct in static T-pose but explode/collapse/float when any retargeted animation plays. There is no Unreal-side workaround for this — the FBX has to be re-exported.

---

## Receiving-side verification (UE programmer, after import)

Open the imported `SK_Char_<Name>` skeleton. Click the `root` bone. Look at Reference Transform → Scale.

- ✓ `(1, 1, 1)` → FBX is good. Proceed with IK Rig + Retargeter setup.
- ✗ Anything else → reject the FBX. Send the artist back to this guide. Do **not** try to fix it in Unreal — every workaround leaks somewhere downstream (verified the hard way).

Also confirm bone names in the hierarchy match the standard list above. Mismatched names break automatic chain mapping in the IK Rig.

---

## Animation deliverables (separate from mesh FBX)

When animations are needed, export them as **separate FBX files**, one anim per file, with the same Transform/Armature settings as above plus:

- **Bake Animation** = ON (only for these files)
- **Force Start/End Keying** = ON
- **Sampling Rate** = 30 (or 60 — must be a snap framerate)
- **Simplify** = 0.0 (no curve simplification — preserves keyframes exactly)

Don't bundle anims into the mesh FBX even if it seems convenient. Separate files keep the mesh import clean and let us re-import or replace anims independently.
