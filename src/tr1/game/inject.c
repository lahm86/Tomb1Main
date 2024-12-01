#include "game/inject.h"

#include "config.h"
#include "game/gamebuf.h"
#include "game/gameflow.h"
#include "game/level.h"
#include "game/output.h"
#include "game/packer.h"
#include "game/room.h"
#include "global/const.h"
#include "global/vars.h"
#include "items.h"

#include <libtrx/benchmark.h>
#include <libtrx/game/level.h>
#include <libtrx/log.h>
#include <libtrx/memory.h>
#include <libtrx/utils.h>
#include <libtrx/virtual_file.h>

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>

#define INJECTION_MAGIC MKTAG('T', '1', 'M', 'J')
#define INJECTION_CURRENT_VERSION 9
#define NULL_FD_INDEX ((uint16_t)(-1))

static bool M_TestRelevancy(const INJECTION *injection);
static bool M_HandleSet(const INJECTION_SET *set);
static bool M_HandleFloorEdit(FLOOR_EDIT_TYPE type, const INJECTION *injection);
static void M_TriggeredItem(const INJECTION *injection);

static uint16_t M_RemapRGB(LEVEL_INFO *level_info, RGB_888 rgb);
static void M_AlignTextureReferences(
    const OBJECT *object, const uint16_t *palette_map, int32_t tex_info_base);

static void M_LoadTexturePages(
    INJECTION *injection, LEVEL_INFO *level_info, uint16_t *palette_map,
    RGBA_8888 *page_ptr);
static void M_TextureData(
    INJECTION *injection, LEVEL_INFO *level_info, int32_t page_base);
static void M_MeshData(const INJECTION *injection, LEVEL_INFO *level_info);
static void M_AnimData(const INJECTION_SET *set);
static void M_ObjectData(
    INJECTION *injection, int32_t data_count, int32_t anim_offset,
    int32_t bone_offset);
static void M_SFXData(const INJECTION_SET *set);

static void M_TextureOverwrites(
    INJECTION *injection, LEVEL_INFO *level_info, uint16_t *palette_map);

static bool M_TestRelevancy(const INJECTION *const injection)
{
    switch (injection->type) {
    case IMT_GENERAL:
    case IMT_LARA_ANIMS:
        return true;
    case IMT_BRAID:
        return g_Config.enable_braid;
    case IMT_UZI_SFX:
        return g_Config.enable_ps_uzi_sfx;
    case IMT_FLOOR_DATA:
        return g_Config.fix_floor_data_issues;
    case IMT_TEXTURE_FIX:
        return g_Config.fix_texture_issues;
    case IMT_LARA_JUMPS:
        return false; // Merged with IMT_LARA_ANIMS in 4.6
    case IMT_ITEM_POSITION:
        return g_Config.fix_item_rots;
    case IMT_PS1_ENEMY:
        return g_Config.restore_ps1_enemies;
    case IMT_DISABLE_ANIM_SPRITE:
        return !g_Config.fix_animated_sprites;
    case IMT_SKYBOX:
        return g_Config.enable_skybox;
    case IMT_PS1_CRYSTAL:
        return g_Config.enable_save_crystals && g_Config.enable_ps1_crystals;
    default:
        return false;
    }
}

static bool M_HandleSet(const INJECTION_SET *const set)
{
    switch (set->type) {
    /*case IST_TEXTURE_DATA:
        break;
    case IST_TEXTURE_INFO:
        break;
    case IST_MESH_DATA:
        break;*/
    case IST_ANIMATION_DATA:
        M_AnimData(set);
        break;
    /*case IST_OBJECT_DATA:
        break;*/
    case IST_SFX_DATA:
        M_SFXData(set);
        break;
    default:
        return false;
    }

    return true;
}

static bool M_HandleFloorEdit(
    const FLOOR_EDIT_TYPE type, const INJECTION *const injection)
{
    if (type != FET_TRIGGER_ITEM) {
        return false;
    }

    M_TriggeredItem(injection);
    return true;
}

static void M_TriggeredItem(const INJECTION *const injection)
{
    if (Item_GetTotalCount() == MAX_ITEMS) {
        VFile_Skip(
            injection->fp,
            sizeof(int16_t) * 4 + sizeof(int32_t) * 3 + sizeof(uint16_t));
        LOG_WARNING("Cannot add more than %d items", MAX_ITEMS);
        return;
    }

    const int16_t item_num = Item_Create();
    ITEM *const item = Item_Get(item_num);

    item->object_id = VFile_ReadS16(injection->fp);
    item->room_num = VFile_ReadS16(injection->fp);
    item->pos.x = VFile_ReadS32(injection->fp);
    item->pos.y = VFile_ReadS32(injection->fp);
    item->pos.z = VFile_ReadS32(injection->fp);
    item->rot.y = VFile_ReadS16(injection->fp);
    item->shade = VFile_ReadS16(injection->fp);
    item->flags = VFile_ReadU16(injection->fp);

    LEVEL_INFO *const level_info = Level_GetInfo();
    level_info->item_count++;
    g_LevelItemCount++;
}

static void M_LoadTexturePages(
    INJECTION *injection, LEVEL_INFO *level_info, uint16_t *palette_map,
    RGBA_8888 *page_ptr)
{
    BENCHMARK *const benchmark = Benchmark_Start();

    /*INJECTION_INFO *inj_info = injection->info;
    VFILE *const fp = injection->fp;

    palette_map[0] = 0;
    RGB_888 source_palette[256];
    for (int32_t i = 0; i < 256; i++) {
        source_palette[i].r = VFile_ReadU8(fp);
        source_palette[i].g = VFile_ReadU8(fp);
        source_palette[i].b = VFile_ReadU8(fp);
    }
    for (int32_t i = 1; i < 256; i++) {
        source_palette[i].r *= 4;
        source_palette[i].g *= 4;
        source_palette[i].b *= 4;
    }
    for (int32_t i = 0; i < 256; i++) {
        palette_map[i] = M_RemapRGB(level_info, source_palette[i]);
    }

    // Read in each page for this injection and realign the pixels
    // to the level's palette.
    const size_t pixel_count = PAGE_SIZE * inj_info->texture_page_count;
    uint8_t *indices = Memory_Alloc(pixel_count);
    VFile_Read(fp, indices, pixel_count);
    uint8_t *input = indices;
    RGBA_8888 *output = page_ptr;
    for (size_t i = 0; i < pixel_count; i++) {
        const uint8_t index = *input++;
        if (index == 0) {
            output->a = 0;
        } else {
            output->r = source_palette[index].r;
            output->g = source_palette[index].g;
            output->b = source_palette[index].b;
            output->a = 255;
        }
        output++;
    }
    Memory_FreePointer(&indices);*/

    Benchmark_End(benchmark, NULL);
}

static void M_TextureData(
    INJECTION *injection, LEVEL_INFO *level_info, int32_t page_base)
{
    BENCHMARK *const benchmark = Benchmark_Start();

    /*INJECTION_INFO *inj_info = injection->info;
    VFILE *const fp = injection->fp;

    // Read the tex_infos and align them to the end of the page list.
    for (int32_t i = 0; i < inj_info->texture_count; i++) {
        PHD_TEXTURE *texture = &g_PhdTextureInfo[level_info->texture_count + i];
        texture->drawtype = VFile_ReadU16(fp);
        texture->tpage = VFile_ReadU16(fp);
        for (int32_t j = 0; j < 4; j++) {
            texture->uv[j].u = VFile_ReadU16(fp);
            texture->uv[j].v = VFile_ReadU16(fp);
        }
        g_PhdTextureInfo[level_info->texture_count + i].tpage += page_base;
    }

    for (int32_t i = 0; i < inj_info->sprite_info_count; i++) {
        PHD_SPRITE *sprite =
            &g_PhdSpriteInfo[level_info->sprite_info_count + i];
        sprite->tpage = VFile_ReadU16(fp);
        sprite->offset = VFile_ReadU16(fp);
        sprite->width = VFile_ReadU16(fp);
        sprite->height = VFile_ReadU16(fp);
        sprite->x1 = VFile_ReadS16(fp);
        sprite->y1 = VFile_ReadS16(fp);
        sprite->x2 = VFile_ReadS16(fp);
        sprite->y2 = VFile_ReadS16(fp);
        g_PhdSpriteInfo[level_info->sprite_info_count + i].tpage += page_base;
    }

    for (int32_t i = 0; i < inj_info->sprite_count; i++) {
        const GAME_OBJECT_ID object_id = VFile_ReadS32(fp);
        const int16_t num_meshes = VFile_ReadS16(fp);
        const int16_t mesh_idx = VFile_ReadS16(fp);

        if (object_id < O_NUMBER_OF) {
            OBJECT *object = &g_Objects[object_id];
            object->nmeshes = num_meshes;
            object->mesh_idx = mesh_idx + level_info->sprite_info_count;
            object->loaded = 1;
        } else if (object_id - O_NUMBER_OF < STATIC_NUMBER_OF) {
            STATIC_INFO *object = &g_StaticObjects[object_id - O_NUMBER_OF];
            object->nmeshes = num_meshes;
            object->mesh_num = mesh_idx + level_info->sprite_info_count;
            object->loaded = true;
        }
        level_info->sprite_info_count -= num_meshes;
        level_info->sprite_count++;
    }*/

    Benchmark_End(benchmark, NULL);
}

static void M_MeshData(const INJECTION *injection, LEVEL_INFO *const level_info)
{
    BENCHMARK *const benchmark = Benchmark_Start();

    /*const INJECTION_INFO *const inj_info = injection->info;
    VFILE *const fp = injection->fp;

    const int32_t mesh_base = level_info->mesh_count;
    const size_t data_start_pos = VFile_GetPos(fp);
    VFile_Skip(fp, inj_info->mesh_count * sizeof(int16_t));
    level_info->mesh_count += inj_info->mesh_count;

    const int32_t alloc_size = inj_info->mesh_ptr_count * sizeof(int32_t);
    int32_t *mesh_indices = Memory_Alloc(alloc_size);
    VFile_Read(fp, mesh_indices, alloc_size);

    const size_t end_pos = VFile_GetPos(fp);
    VFile_SetPos(fp, data_start_pos);

    Level_ReadObjectMeshes(inj_info->mesh_ptr_count, mesh_indices, fp);

    VFile_SetPos(fp, end_pos);
    Memory_FreePointer(&mesh_indices);*/

    Benchmark_End(benchmark, NULL);
}

static int32_t M_AnimChanges(
    const INJECTION *const injection, const int32_t data_count)
{
    LEVEL_INFO *const level_info = Level_GetInfo();
    for (int32_t i = 0; i < data_count; i++) {
        ANIM_CHANGE *const anim_change =
            &g_AnimChanges[level_info->anim_change_count + i];
        anim_change->goal_anim_state = VFile_ReadS16(injection->fp);
        anim_change->num_ranges = VFile_ReadS16(injection->fp);
        anim_change->range_idx = VFile_ReadS16(injection->fp);
        anim_change->range_idx += level_info->anim_range_count;
    }

    const int32_t current_offset = level_info->anim_change_count;
    level_info->anim_change_count += data_count;
    return current_offset;
}

static int32_t M_AnimRanges(
    const INJECTION *const injection, const int32_t data_count)
{
    LEVEL_INFO *const level_info = Level_GetInfo();
    for (int32_t i = 0; i < data_count; i++) {
        ANIM_RANGE *const anim_range =
            &g_AnimRanges[level_info->anim_range_count + i];
        anim_range->start_frame = VFile_ReadS16(injection->fp);
        anim_range->end_frame = VFile_ReadS16(injection->fp);
        anim_range->link_anim_num = VFile_ReadS16(injection->fp);
        anim_range->link_frame_num = VFile_ReadS16(injection->fp);
        anim_range->link_anim_num += level_info->anim_count;
    }

    const int32_t current_offset = level_info->anim_range_count;
    level_info->anim_range_count += data_count;
    return current_offset;
}

static int32_t M_AnimCommands(
    const INJECTION *const injection, const int32_t data_count)
{
    LEVEL_INFO *const level_info = Level_GetInfo();
    VFile_Read(
        injection->fp, g_AnimCommands + level_info->anim_command_count,
        sizeof(int16_t) * data_count);

    const int32_t current_offset = level_info->anim_command_count;
    level_info->anim_command_count += data_count;
    return current_offset;
}

static int32_t M_AnimBones(
    const INJECTION *const injection, const int32_t data_count)
{
    LEVEL_INFO *const level_info = Level_GetInfo();
    VFile_Read(
        injection->fp, g_AnimBones + level_info->anim_bone_count,
        sizeof(int32_t) * data_count);

    const int32_t current_offset = level_info->anim_bone_count;
    level_info->anim_bone_count += data_count;
    return current_offset;
}

static void M_AnimFrames(
    const INJECTION *const injection, const int32_t data_count)
{
    LEVEL_INFO *const level_info = Level_GetInfo();

    const size_t frame_data_start = VFile_GetPos(injection->fp);
    VFile_Skip(injection->fp, data_count * sizeof(int16_t));
    const size_t frame_data_end = VFile_GetPos(injection->fp);

    VFile_SetPos(injection->fp, frame_data_start);
    int32_t *mesh_rots =
        &g_AnimFrameMeshRots[level_info->anim_frame_mesh_rot_count];
    int32_t i = 0;
    while (VFile_GetPos(injection->fp) < frame_data_end) {
        level_info->anim_frame_offsets[level_info->anim_frame_count + i] =
            VFile_GetPos(injection->fp) - frame_data_start;
        FRAME_INFO *const frame =
            &g_AnimFrames[level_info->anim_frame_count + i];
        frame->bounds.min.x = VFile_ReadS16(injection->fp);
        frame->bounds.max.x = VFile_ReadS16(injection->fp);
        frame->bounds.min.y = VFile_ReadS16(injection->fp);
        frame->bounds.max.y = VFile_ReadS16(injection->fp);
        frame->bounds.min.z = VFile_ReadS16(injection->fp);
        frame->bounds.max.z = VFile_ReadS16(injection->fp);
        frame->offset.x = VFile_ReadS16(injection->fp);
        frame->offset.y = VFile_ReadS16(injection->fp);
        frame->offset.z = VFile_ReadS16(injection->fp);
        frame->nmeshes = VFile_ReadS16(injection->fp);
        frame->mesh_rots = mesh_rots;
        VFile_Read(injection->fp, mesh_rots, sizeof(int32_t) * frame->nmeshes);
        mesh_rots += frame->nmeshes;
    }

    assert(VFile_GetPos(injection->fp) == frame_data_end);

    // TODO: we don't increment level info counts currently?
}

static int32_t M_Animations(
    const INJECTION *const injection, const int32_t data_count,
    const int32_t change_offset, const int32_t command_offset)
{
    LEVEL_INFO *const level_info = Level_GetInfo();
    for (int32_t i = 0; i < data_count; i++) {
        ANIM *const anim = &g_Anims[level_info->anim_count + i];

        anim->frame_ofs = VFile_ReadU32(injection->fp);
        anim->interpolation = VFile_ReadS16(injection->fp);
        anim->current_anim_state = VFile_ReadS16(injection->fp);
        anim->velocity = VFile_ReadS32(injection->fp);
        anim->acceleration = VFile_ReadS32(injection->fp);
        anim->frame_base = VFile_ReadS16(injection->fp);
        anim->frame_end = VFile_ReadS16(injection->fp);
        anim->jump_anim_num = VFile_ReadS16(injection->fp);
        anim->jump_frame_num = VFile_ReadS16(injection->fp);
        anim->num_changes = VFile_ReadS16(injection->fp);
        anim->change_idx = VFile_ReadS16(injection->fp);
        anim->num_commands = VFile_ReadS16(injection->fp);
        anim->command_idx = VFile_ReadS16(injection->fp);

        // Re-align to the level.
        anim->jump_anim_num += level_info->anim_count;
        /*bool found = false;
        for (int32_t j = 0; j < inj_info->anim_frame_count; j++) {
            if (level_info->anim_frame_offsets[level_info->anim_frame_count + j]
                == (signed)anim->frame_ofs) {
                anim->frame_ptr =
                    &g_AnimFrames[level_info->anim_frame_count + j];
                found = true;
                break;
            }
        }*/
        anim->frame_ptr = &g_AnimFrames[level_info->anim_frame_count + 0];
        anim->frame_ofs += level_info->anim_frame_data_count * 2;
        // assert(found);
        if (anim->num_changes) {
            anim->change_idx += change_offset;
        }
        if (anim->num_commands) {
            anim->command_idx += command_offset;
        }
    }

    const int32_t current_offset = level_info->anim_count;
    level_info->anim_count += data_count;
    return current_offset;
}

static void M_AnimData(const INJECTION_SET *const set)
{
    BENCHMARK *const benchmark = Benchmark_Start();

    VFILE *const file = set->injection->fp;
    int32_t change_offset = 0;
    int32_t cmd_offset = 0;
    int32_t bone_offset = 0;
    int32_t anim_offset = 0;

    for (int32_t i = 0; i < set->num_blocks; i++) {
        const INJECTION_DATA_TYPE block_type = VFile_ReadS32(file);
        const int32_t data_count = VFile_ReadS32(file);
        const int32_t data_size = VFile_ReadS32(file);

        switch (block_type) {
        case IDT_ANIM_CHANGES:
            change_offset = M_AnimChanges(set->injection, data_count);
            break;
        case IDT_ANIM_RANGES:
            M_AnimRanges(set->injection, data_count);
            break;
        case IDT_ANIM_COMMANDS:
            cmd_offset += M_AnimCommands(set->injection, data_count);
            break;
        case IDT_ANIM_BONES:
            bone_offset += M_AnimBones(set->injection, data_count);
            break;
        case IDT_ANIM_FRAMES:
            M_AnimFrames(set->injection, data_count);
            break;
        case IDT_ANIMS:
            anim_offset = M_Animations(
                set->injection, data_count, change_offset, cmd_offset);
            break;
        case IDT_OBJECTS:
            M_ObjectData(set->injection, data_count, anim_offset, bone_offset);
            break;
        default:
            VFile_Skip(file, data_size);
            break;
        }
    }

    Benchmark_End(benchmark, NULL);
}

static void M_ObjectData(
    INJECTION *injection, const int32_t data_count, const int32_t anim_offset,
    const int32_t bone_offset)
{
    BENCHMARK *const benchmark = Benchmark_Start();

    // TODO: shift into common
    const LEVEL_INFO *const level_info = Level_GetInfo();
    for (int32_t i = 0; i < data_count; i++) {
        OBJECT *const object = Object_GetObject(VFile_ReadS32(injection->fp));

        const int16_t num_meshes = VFile_ReadS16(injection->fp);
        const int16_t mesh_idx = VFile_ReadS16(injection->fp);
        const int32_t bone_idx = VFile_ReadS32(injection->fp);

        // When mesh data has been omitted from the injection, this indicates
        // that we wish to retain what's already defined so to avoid duplicate
        // packing.
        if (!object->loaded || num_meshes != 0) {
            object->nmeshes = num_meshes;
            object->mesh_idx =
                mesh_idx; // + level_info->mesh_ptr_count; need meshes handled
            object->bone_idx = bone_idx + bone_offset;
        }

        const int32_t frame_offset = VFile_ReadS32(injection->fp);
        object->anim_idx = VFile_ReadS16(injection->fp);
        // object->anim_idx += anim_offset;
        object->loaded = 1;

        object->frame_base = &g_AnimFrames[level_info->anim_frame_count + 0];

        /*bool found = false;
        for (int32_t j = 0; j < inj_info->anim_frame_count; j++) {
            if (level_info
                    ->anim_frame_offsets[level_info->anim_frame_count + j]
                == frame_offset) {
                object->frame_base =
                    &g_AnimFrames[level_info->anim_frame_count + j];
                found = true;
                break;
            }
        }*/
        //    assert(found);

        if (num_meshes) {
            /*M_AlignTextureReferences(
                object, palette_map, level_info->texture_count);*/
        }
    }

    Benchmark_End(benchmark, NULL);
}

static void M_SFXData(const INJECTION_SET *const set)
{
    BENCHMARK *const benchmark = Benchmark_Start();

    assert(set->num_blocks == 1);

    VFILE *const file = set->injection->fp;
    VFile_Skip(file, sizeof(int32_t));
    const int32_t data_count = VFile_ReadS32(file);
    LOG_DEBUG("%d", data_count);
    VFile_Skip(file, sizeof(int32_t));

    LEVEL_INFO *const level_info = Level_GetInfo();
    for (int32_t i = 0; i < data_count; i++) {
        const int16_t sfx_id = VFile_ReadS16(file);
        g_SampleLUT[sfx_id] = level_info->sample_info_count;

        SAMPLE_INFO *const sample_info =
            &g_SampleInfos[level_info->sample_info_count];
        sample_info->volume = VFile_ReadS16(file);
        sample_info->randomness = VFile_ReadS16(file);
        sample_info->flags = VFile_ReadS16(file);
        sample_info->number = level_info->sample_count;

        const int16_t num_samples = (sample_info->flags >> 2) & 15;
        for (int32_t j = 0; j < num_samples; j++) {
            const int32_t sample_length = VFile_ReadS32(file);
            VFile_Read(
                file, level_info->sample_data + level_info->sample_data_size,
                sizeof(char) * sample_length);

            level_info->sample_offsets[level_info->sample_count] =
                level_info->sample_data_size;
            level_info->sample_data_size += sample_length;
            level_info->sample_count++;
        }

        level_info->sample_info_count++;
    }

    Benchmark_End(benchmark, NULL);
}

static void M_AlignTextureReferences(
    const OBJECT *const object, const uint16_t *const palette_map,
    const int32_t tex_info_base)
{
    /*for (int32_t i = 0; i < object->nmeshes; i++) {
        OBJECT_MESH *const mesh = Object_GetMesh(object->mesh_idx + i);
        for (int32_t j = 0; j < mesh->num_tex_face4s; j++) {
            mesh->tex_face4s[j].texture += tex_info_base;
        }

        for (int32_t j = 0; j < mesh->num_tex_face3s; j++) {
            mesh->tex_face3s[j].texture += tex_info_base;
        }

        for (int32_t j = 0; j < mesh->num_flat_face4s; j++) {
            FACE4 *const face = &mesh->flat_face4s[j];
            face->texture = palette_map[face->texture];
        }

        for (int32_t j = 0; j < mesh->num_flat_face3s; j++) {
            FACE3 *const face = &mesh->flat_face3s[j];
            face->texture = palette_map[face->texture];
        }
    }*/
}

static uint16_t M_RemapRGB(LEVEL_INFO *level_info, RGB_888 rgb)
{
    // Find the index of the exact match to the given RGB
    for (int32_t i = 0; i < level_info->palette_size; i++) {
        const RGB_888 test_rgb = level_info->palette[i];
        if (rgb.r == test_rgb.r && rgb.g == test_rgb.g && rgb.b == test_rgb.b) {
            return i;
        }
    }

    // Match not found - expand the game palette
    level_info->palette_size++;
    level_info->palette = Memory_Realloc(
        level_info->palette, level_info->palette_size * sizeof(RGB_888));
    level_info->palette[level_info->palette_size - 1] = rgb;
    return level_info->palette_size - 1;
}

static void M_TextureOverwrites(
    INJECTION *injection, LEVEL_INFO *level_info, uint16_t *palette_map)
{
    BENCHMARK *const benchmark = Benchmark_Start();

    /*INJECTION_INFO *inj_info = injection->info;
    VFILE *const fp = injection->fp;

    for (int32_t i = 0; i < inj_info->texture_overwrite_count; i++) {
        const uint16_t target_page = VFile_ReadU16(fp);
        const uint8_t target_x = VFile_ReadU8(fp);
        const uint8_t target_y = VFile_ReadU8(fp);
        const uint16_t source_width = VFile_ReadU16(fp);
        const uint16_t source_height = VFile_ReadU16(fp);

        uint8_t *source_img = Memory_Alloc(source_width * source_height);
        VFile_Read(fp, source_img, source_width * source_height);

        // Copy the source image pixels directly into the target page.
        RGBA_8888 *page =
            level_info->texture_rgb_page_ptrs + target_page * PAGE_SIZE;
        for (int32_t y = 0; y < source_height; y++) {
            for (int32_t x = 0; x < source_width; x++) {
                const uint8_t pal_idx = source_img[y * source_width + x];
                const int32_t target_pixel =
                    (y + target_y) * PAGE_WIDTH + x + target_x;
                if (pal_idx == 0) {
                    (*(page + target_pixel)).a = 0;
                } else {
                    const RGB_888 pix =
                        level_info->palette[palette_map[pal_idx]];
                    (*(page + target_pixel)).r = pix.r;
                    (*(page + target_pixel)).g = pix.g;
                    (*(page + target_pixel)).b = pix.b;
                    (*(page + target_pixel)).a = 255;
                }
            }
        }

        Memory_FreePointer(&source_img);
    }*/

    Benchmark_End(benchmark, NULL);
}

void Inject_InitLevel(const int32_t level_num)
{
    INJECT_INTERFACE interface = {
        .is_relevant = M_TestRelevancy,
        .handle_set = M_HandleSet,
        .handle_floor_edit = M_HandleFloorEdit,
    };

    const GAMEFLOW_LEVEL *const level = &g_GameFlow.levels[level_num];
    const INJECTION_ARGS args = {
        .num_files = level->injections.length,
        .files = level->injections.data_paths,
        .interface = &interface,
    };

    Inject_Init(&args);
}

static void M_OldAllInjections(LEVEL_INFO *level_info)
{
    /*if (!m_Injections) {
        return;
    }

    BENCHMARK *const benchmark = Benchmark_Start();

    uint16_t palette_map[256];
    RGBA_8888 *source_pages = Memory_Alloc(
        m_Aggregate->texture_page_count * PAGE_SIZE * sizeof(RGBA_8888));
    int32_t source_page_count = 0;
    int32_t tpage_base = level_info->texture_page_count;

    for (int32_t i = 0; i < m_NumInjections; i++) {
        INJECTION *injection = &m_Injections[i];
        if (!injection->relevant) {
            continue;
        }

        M_LoadTexturePages(
            injection, level_info, palette_map,
            source_pages + (source_page_count * PAGE_SIZE));

        M_TextureData(injection, level_info, tpage_base);
        M_MeshData(injection, level_info);
        M_AnimData(injection, level_info);
        M_ObjectData(injection, level_info, palette_map);
        M_SFXData(injection, level_info);

        M_MeshEdits(injection, palette_map);
        M_TextureOverwrites(injection, level_info, palette_map);
        M_FloorDataEdits(injection, level_info);
        M_RoomMeshEdits(injection);
        M_RoomDoorEdits(injection);
        M_AnimRangeEdits(injection);

        M_ItemPositions(injection);

        // Realign base indices for the next injection.
        INJECTION_INFO *inj_info = injection->info;
        level_info->anim_command_count += inj_info->anim_cmd_count;
        level_info->anim_bone_count += inj_info->anim_bone_count;
        level_info->anim_frame_data_count += inj_info->anim_frame_data_count;
        level_info->anim_frame_count += inj_info->anim_frame_count;
        level_info->anim_frame_mesh_rot_count +=
            inj_info->anim_frame_mesh_rot_count;
        level_info->anim_count += inj_info->anim_count;
        level_info->mesh_ptr_count += inj_info->mesh_ptr_count;
        level_info->texture_count += inj_info->texture_count;
        source_page_count += inj_info->texture_page_count;
        tpage_base += inj_info->texture_page_count;
    }

    if (source_page_count) {
        PACKER_DATA *data = Memory_Alloc(sizeof(PACKER_DATA));
        data->level_page_count = level_info->texture_page_count;
        data->source_page_count = source_page_count;
        data->source_pages = source_pages;
        data->level_pages = level_info->texture_rgb_page_ptrs;
        data->object_count = level_info->texture_count;
        data->sprite_count = level_info->sprite_info_count;

        if (Packer_Pack(data)) {
            level_info->texture_page_count += Packer_GetAddedPageCount();
            level_info->texture_rgb_page_ptrs = data->level_pages;
        }

        Memory_FreePointer(&source_pages);
        Memory_FreePointer(&data);
    }

    Benchmark_End(benchmark, NULL);*/
}
