#include "game/lara/draw.h"

#include "game/gun.h"
#include "game/items.h"
#include "game/lara/common.h"
#include "game/lara/hair.h"
#include "game/output.h"
#include "game/viewport.h"
#include "global/vars.h"
#include "math/matrix.h"

#define LIMB_BONE_COUNT 3

static void M_ProcessBone(
    const ANIM_BONE *bones, const int32_t *rotation, LARA_MESH mesh_idx,
    int32_t clip);
static void M_ProcessBone_I(
    const ANIM_BONE *bones, const int32_t *rotations1,
    const int32_t *const rotations2, LARA_MESH mesh_idx, int32_t clip);

static void M_ProcessLimb(
    const ANIM_BONE *bones, const int32_t *rotations, LARA_MESH mesh_idx,
    int32_t clip);
static void M_ProcessLimb_I(
    const ANIM_BONE *bones, const int32_t *rotations1,
    const int32_t *rotations2, LARA_MESH mesh_idx, int32_t clip);

static void M_ProcessPistolArm(
    const ANIM_BONE *bones, LARA_MESH mesh_idx, int32_t clip);
static void M_ProcessPistolArm_I(
    const ANIM_BONE *bones, LARA_MESH mesh_idx, int32_t clip);

static void M_ProcessShotGunArm(
    const ANIM_BONE *bones, LARA_MESH mesh_idx, int32_t clip);
static void M_ProcessShotGunArm_I(
    const ANIM_BONE *bones, LARA_MESH mesh_idx, int32_t clip);

static void M_DrawMesh(LARA_MESH mesh_idx, int32_t clip, bool interpolated);

static void M_Draw_I(
    const ITEM *item, const FRAME_INFO *frame1, const FRAME_INFO *frame2,
    int32_t frac, int32_t rate);

static void M_ProcessBone(
    const ANIM_BONE *const bones, const int32_t *const rotation,
    const LARA_MESH mesh_idx, const int32_t clip)
{
    const ANIM_BONE *bone = &bones[mesh_idx - 1];
    Matrix_TranslateRel(bone->pos.x, bone->pos.y, bone->pos.z);
    Matrix_RotYXZpack(rotation[mesh_idx]);

    if (mesh_idx == LM_TORSO) {
        Matrix_RotYXZ(
            g_Lara.interp.result.torso_rot.y, g_Lara.interp.result.torso_rot.x,
            g_Lara.interp.result.torso_rot.z);
    } else if (mesh_idx == LM_HEAD) {
        Matrix_RotYXZ(
            g_Lara.interp.result.head_rot.y, g_Lara.interp.result.head_rot.x,
            g_Lara.interp.result.head_rot.z);
    }

    M_DrawMesh(mesh_idx, clip, false);
}

static void M_ProcessBone_I(
    const ANIM_BONE *const bones, const int32_t *const rotations1,
    const int32_t *const rotations2, const LARA_MESH mesh_idx,
    const int32_t clip)
{
    const ANIM_BONE *bone = &bones[mesh_idx - 1];
    Matrix_TranslateRel_I(bone->pos.x, bone->pos.y, bone->pos.z);
    Matrix_RotYXZpack_I(rotations1[mesh_idx], rotations2[mesh_idx]);

    if (mesh_idx == LM_TORSO) {
        Matrix_RotYXZ_I(
            g_Lara.interp.result.torso_rot.y, g_Lara.interp.result.torso_rot.x,
            g_Lara.interp.result.torso_rot.z);
    } else if (mesh_idx == LM_HEAD) {
        Matrix_RotYXZ_I(
            g_Lara.interp.result.head_rot.y, g_Lara.interp.result.head_rot.x,
            g_Lara.interp.result.head_rot.z);
    }

    M_DrawMesh(mesh_idx, clip, true);
}

static void M_ProcessLimb(
    const ANIM_BONE *const bones, const int32_t *const rotations,
    const LARA_MESH mesh_idx, const int32_t clip)
{
    for (int32_t i = 0; i < LIMB_BONE_COUNT; i++) {
        M_ProcessBone(bones, rotations, mesh_idx + i, clip);
    }
}

static void M_ProcessLimb_I(
    const ANIM_BONE *const bones, const int32_t *const rotations1,
    const int32_t *const rotations2, const LARA_MESH mesh_idx,
    const int32_t clip)
{
    for (int32_t i = 0; i < LIMB_BONE_COUNT; i++) {
        M_ProcessBone_I(bones, rotations1, rotations2, mesh_idx + i, clip);
    }
}

static void M_ProcessPistolArm(
    const ANIM_BONE *const bones, const LARA_MESH mesh_idx, const int32_t clip)
{
    const ANIM_BONE *bone = &bones[mesh_idx - 1];
    Matrix_TranslateRel(bone->pos.x, bone->pos.y, bone->pos.z);

    g_MatrixPtr->_00 = g_MatrixPtr[-2]._00;
    g_MatrixPtr->_01 = g_MatrixPtr[-2]._01;
    g_MatrixPtr->_02 = g_MatrixPtr[-2]._02;
    g_MatrixPtr->_10 = g_MatrixPtr[-2]._10;
    g_MatrixPtr->_11 = g_MatrixPtr[-2]._11;
    g_MatrixPtr->_12 = g_MatrixPtr[-2]._12;
    g_MatrixPtr->_20 = g_MatrixPtr[-2]._20;
    g_MatrixPtr->_21 = g_MatrixPtr[-2]._21;
    g_MatrixPtr->_22 = g_MatrixPtr[-2]._22;

    const LARA_ARM arm =
        mesh_idx == LM_UARM_L ? g_Lara.left_arm : g_Lara.right_arm;
    const int32_t *const rotation = arm.frame_base[arm.frame_num].mesh_rots;
    Matrix_RotYXZ(
        arm.interp.result.rot.y, arm.interp.result.rot.x,
        arm.interp.result.rot.z);
    Matrix_RotYXZpack(rotation[mesh_idx]);
    M_DrawMesh(mesh_idx, clip, false);

    M_ProcessBone(bones, rotation, mesh_idx + 1, clip);
    M_ProcessBone(bones, rotation, mesh_idx + 2, clip);
}

static void M_ProcessPistolArm_I(
    const ANIM_BONE *const bones, const LARA_MESH mesh_idx, const int32_t clip)
{
    const ANIM_BONE *bone = &bones[mesh_idx - 1];
    Matrix_TranslateRel_I(bone->pos.x, bone->pos.y, bone->pos.z);
    Matrix_InterpolateArm();

    const LARA_ARM arm =
        mesh_idx == LM_UARM_L ? g_Lara.left_arm : g_Lara.right_arm;
    const int32_t *const rotation = arm.frame_base[arm.frame_num].mesh_rots;
    Matrix_RotYXZ(
        arm.interp.result.rot.y, arm.interp.result.rot.x,
        arm.interp.result.rot.z);
    Matrix_RotYXZpack(rotation[mesh_idx]);
    M_DrawMesh(mesh_idx, clip, false);

    M_ProcessBone(bones, rotation, mesh_idx + 1, clip);
    M_ProcessBone(bones, rotation, mesh_idx + 2, clip);
}

static void M_ProcessShotGunArm(
    const ANIM_BONE *const bones, const LARA_MESH mesh_idx, const int32_t clip)
{
    const LARA_ARM arm =
        mesh_idx == LM_UARM_L ? g_Lara.left_arm : g_Lara.right_arm;
    const int32_t *const rotations = arm.frame_base[arm.frame_num].mesh_rots;

    M_ProcessLimb(bones, rotations, mesh_idx, clip);
}

static void M_ProcessShotGunArm_I(
    const ANIM_BONE *const bones, const LARA_MESH mesh_idx, const int32_t clip)
{
    const LARA_ARM arm =
        mesh_idx == LM_UARM_L ? g_Lara.left_arm : g_Lara.right_arm;
    const int32_t *const rotations = arm.frame_base[arm.frame_num].mesh_rots;

    M_ProcessLimb_I(bones, rotations, rotations, mesh_idx, clip);
}

static void M_DrawMesh(
    const LARA_MESH mesh_idx, const int32_t clip, const bool interpolated)
{
    const OBJECT_MESH *const mesh = Lara_GetMesh(mesh_idx);
    if (interpolated) {
        Output_DrawObjectMesh_I(mesh, clip);
    } else {
        Output_DrawObjectMesh(mesh, clip);
    }
}

static void M_Draw_I(
    const ITEM *const item, const FRAME_INFO *const frame1,
    const FRAME_INFO *const frame2, const int32_t frac, const int32_t rate)
{
    const OBJECT *const object = Object_GetObject(item->object_id);
    const BOUNDS_16 *const bounds = Item_GetBoundsAccurate(item);
    MATRIX saved_matrix = *g_MatrixPtr;

    Output_DrawShadow(object->shadow_size, bounds, item);

    Matrix_Push();
    Matrix_TranslateAbs(
        item->interp.result.pos.x, item->interp.result.pos.y,
        item->interp.result.pos.z);
    Matrix_RotYXZ(
        item->interp.result.rot.y, item->interp.result.rot.x,
        item->interp.result.rot.z);

    const int32_t clip = Output_GetObjectBounds(&frame1->bounds);
    if (!clip) {
        Matrix_Pop();
        return;
    }

    Matrix_Push();

    Output_CalculateObjectLighting(item, &frame1->bounds);

    const ANIM_BONE *const bones = Object_GetBone(object);
    const int32_t *const rotations1 = frame1->mesh_rots;
    const int32_t *const rotations2 = frame2->mesh_rots;

    Matrix_InitInterpolate(frac, rate);

    Matrix_TranslateRel_ID(
        frame1->offset.x, frame1->offset.y, frame1->offset.z, frame2->offset.x,
        frame2->offset.y, frame2->offset.z);

    Matrix_RotYXZpack_I(rotations1[LM_HIPS], rotations2[LM_HIPS]);
    M_DrawMesh(LM_HIPS, clip, true);

    Matrix_Push_I();
    M_ProcessLimb_I(bones, rotations1, rotations2, LM_THIGH_L, clip);
    Matrix_Pop_I();

    Matrix_Push_I();
    M_ProcessLimb_I(bones, rotations1, rotations2, LM_THIGH_R, clip);
    Matrix_Pop_I();

    M_ProcessBone_I(bones, rotations1, rotations2, LM_TORSO, clip);

    Matrix_Push_I();
    M_ProcessBone_I(bones, rotations1, rotations2, LM_HEAD, clip);
    *g_MatrixPtr = saved_matrix;
    Lara_Hair_Draw();
    Matrix_Pop_I();

    LARA_GUN_TYPE fire_arms = LGT_UNARMED;
    if (g_Lara.gun_status == LGS_READY || g_Lara.gun_status == LGS_DRAW
        || g_Lara.gun_status == LGS_UNDRAW) {
        fire_arms = g_Lara.gun_type;
    }

    switch (fire_arms) {
    case LGT_UNARMED:
        Matrix_Push_I();
        M_ProcessLimb_I(bones, rotations1, rotations2, LM_UARM_R, clip);
        Matrix_Pop_I();

        Matrix_Push_I();
        M_ProcessLimb_I(bones, rotations1, rotations2, LM_UARM_L, clip);
        Matrix_Pop_I();
        break;

    case LGT_PISTOLS:
    case LGT_MAGNUMS:
    case LGT_UZIS:
        Matrix_Push_I();
        M_ProcessPistolArm_I(bones, LM_UARM_R, clip);
        if (g_Lara.right_arm.flash_gun) {
            saved_matrix = *g_MatrixPtr;
        }
        Matrix_Pop_I();

        Matrix_Push_I();
        M_ProcessPistolArm_I(bones, LM_UARM_L, clip);
        if (g_Lara.left_arm.flash_gun) {
            Gun_DrawFlash(fire_arms, clip);
        }
        if (g_Lara.right_arm.flash_gun) {
            *g_MatrixPtr = saved_matrix;
            Gun_DrawFlash(fire_arms, clip);
        }
        Matrix_Pop_I();
        break;

    case LGT_SHOTGUN:
        Matrix_Push_I();
        M_ProcessShotGunArm_I(bones, LM_UARM_R, clip);
        if (g_Lara.right_arm.flash_gun) {
            saved_matrix = *g_MatrixPtr;
        }
        Matrix_Pop_I();

        Matrix_Push_I();
        M_ProcessShotGunArm_I(bones, LM_UARM_L, clip);
        if (g_Lara.right_arm.flash_gun) {
            *g_MatrixPtr = saved_matrix;
            Gun_DrawFlash(fire_arms, clip);
        }
        Matrix_Pop_I();
        break;

    default:
        break;
    }

    Matrix_Pop();
    Matrix_Pop();
}

void Lara_Draw(ITEM *item)
{
    if (g_LaraItem->flags & IF_INVISIBLE) {
        return;
    }

    const int32_t top = g_PhdTop;
    const int32_t left = g_PhdLeft;
    const int32_t bottom = g_PhdBottom;
    const int32_t right = g_PhdRight;

    g_PhdLeft = Viewport_GetMinX();
    g_PhdTop = Viewport_GetMinY();
    g_PhdBottom = Viewport_GetMaxY();
    g_PhdRight = Viewport_GetMaxX();

    FRAME_INFO *frmptr[2];
    if (g_Lara.hit_direction < 0) {
        int32_t rate;
        const int32_t frac = Item_GetFrames(item, frmptr, &rate);
        if (frac) {
            M_Draw_I(item, frmptr[0], frmptr[1], frac, rate);
            goto end;
        }
    }

    FRAME_INFO *frame;
    const OBJECT *const object = Object_GetObject(item->object_id);
    if (g_Lara.hit_direction >= 0) {
        switch (g_Lara.hit_direction) {
        default:
        case DIR_NORTH:
            frame = Anim_GetAnim(object->anim_idx + LA_SPAZ_FORWARD)->frame_ptr;
            break;
        case DIR_EAST:
            frame = Anim_GetAnim(object->anim_idx + LA_SPAZ_RIGHT)->frame_ptr;
            break;
        case DIR_SOUTH:
            frame = Anim_GetAnim(object->anim_idx + LA_SPAZ_BACK)->frame_ptr;
            break;
        case DIR_WEST:
            frame = Anim_GetAnim(object->anim_idx + LA_SPAZ_LEFT)->frame_ptr;
            break;
        }

        frame += g_Lara.hit_frame;
    } else {
        frame = frmptr[0];
    }

    // save matrix for hair
    MATRIX saved_matrix = *g_MatrixPtr;

    Output_DrawShadow(object->shadow_size, &frame->bounds, item);

    Matrix_Push();
    Matrix_TranslateAbs(
        item->interp.result.pos.x, item->interp.result.pos.y,
        item->interp.result.pos.z);
    Matrix_RotYXZ(
        item->interp.result.rot.y, item->interp.result.rot.x,
        item->interp.result.rot.z);

    const int32_t clip = Output_GetObjectBounds(&frame->bounds);
    if (!clip) {
        Matrix_Pop();
        return;
    }

    Matrix_Push();

    Output_CalculateObjectLighting(item, &frame->bounds);

    const ANIM_BONE *const bones = Object_GetBone(object);
    const int32_t *const rotation = frame->mesh_rots;

    Matrix_TranslateRel(frame->offset.x, frame->offset.y, frame->offset.z);
    Matrix_RotYXZpack(rotation[LM_HIPS]);
    M_DrawMesh(LM_HIPS, clip, false);

    Matrix_Push();
    M_ProcessLimb(bones, rotation, LM_THIGH_L, clip);
    Matrix_Pop();

    Matrix_Push();
    M_ProcessLimb(bones, rotation, LM_THIGH_R, clip);
    Matrix_Pop();

    M_ProcessBone(bones, rotation, LM_TORSO, clip);

    Matrix_Push();
    M_ProcessBone(bones, rotation, LM_HEAD, clip);
    *g_MatrixPtr = saved_matrix;
    Lara_Hair_Draw();
    Matrix_Pop();

    LARA_GUN_TYPE fire_arms = LGT_UNARMED;
    if (g_Lara.gun_status == LGS_READY || g_Lara.gun_status == LGS_DRAW
        || g_Lara.gun_status == LGS_UNDRAW) {
        fire_arms = g_Lara.gun_type;
    }

    switch (fire_arms) {
    case LGT_UNARMED:
        Matrix_Push();
        M_ProcessLimb(bones, rotation, LM_UARM_R, clip);
        Matrix_Pop();

        Matrix_Push();
        M_ProcessLimb(bones, rotation, LM_UARM_L, clip);
        Matrix_Pop();
        break;

    case LGT_PISTOLS:
    case LGT_MAGNUMS:
    case LGT_UZIS:
        Matrix_Push();
        M_ProcessPistolArm(bones, LM_UARM_R, clip);
        if (g_Lara.right_arm.flash_gun) {
            saved_matrix = *g_MatrixPtr;
        }
        Matrix_Pop();

        Matrix_Push();
        M_ProcessPistolArm(bones, LM_UARM_L, clip);
        if (g_Lara.left_arm.flash_gun) {
            Gun_DrawFlash(fire_arms, clip);
        }
        if (g_Lara.right_arm.flash_gun) {
            *g_MatrixPtr = saved_matrix;
            Gun_DrawFlash(fire_arms, clip);
        }
        Matrix_Pop();
        break;

    case LGT_SHOTGUN:
        Matrix_Push();
        M_ProcessShotGunArm(bones, LM_UARM_R, clip);
        if (g_Lara.right_arm.flash_gun) {
            saved_matrix = *g_MatrixPtr;
        }
        Matrix_Pop();

        Matrix_Push();
        M_ProcessShotGunArm(bones, LM_UARM_L, clip);
        if (g_Lara.right_arm.flash_gun) {
            *g_MatrixPtr = saved_matrix;
            Gun_DrawFlash(fire_arms, clip);
        }
        Matrix_Pop();
        break;

    default:
        break;
    }

    Matrix_Pop();
    Matrix_Pop();

end:
    g_PhdLeft = left;
    g_PhdRight = right;
    g_PhdTop = top;
    g_PhdBottom = bottom;
}
