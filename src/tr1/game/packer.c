#include "game/packer.h"

#include "global/const.h"
#include "global/types.h"
#include "global/vars.h"

#include <libtrx/log.h>
#include <libtrx/memory.h>
#include <libtrx/utils.h>

#include <stddef.h>
#include <string.h>

typedef enum {
    RC_EQUALS = 0,
    RC_CONTAINS = 1,
    RC_COVERS = 2,
    RC_UNRELATED = 3
} RECTANGLE_COMPARISON;

typedef struct {
    uint16_t x;
    uint16_t y;
    uint16_t w;
    uint16_t h;
} RECTANGLE;

typedef struct {
    uint16_t index;
    uint16_t tpage;
    RECTANGLE *bounds;
    void (*move)(
        int index, RECTANGLE *old_bounds, uint16_t tpage, uint16_t new_x,
        uint16_t new_y);
} TEX_INFO;

typedef struct {
    int32_t size;
    RECTANGLE *bounds;
    TEX_INFO *tex_infos;
} TEX_CONTAINER;

typedef struct {
    int32_t index;
    int32_t free_space;
    RGBA_8888 data[PAGE_SIZE];
} TEX_PAGE;

static void M_AllocateNewPage(void);
static void M_FillVirtualData(TEX_PAGE *page, RECTANGLE *bounds);
static void M_Cleanup(void);

static RECTANGLE_COMPARISON M_Compare(RECTANGLE *r1, RECTANGLE *r2);
static bool M_EnqueueTexInfo(TEX_INFO *info);
static RECTANGLE *M_GetObjectBounds(const OBJECT_TEXTURE *texture);
static RECTANGLE *M_GetSpriteBounds(const SPRITE_TEXTURE *texture);
static void M_PrepareObject(int object_index);
static void M_PrepareSprite(int sprite_index);

static void M_MoveObject(
    int index, RECTANGLE *old_bounds, uint16_t tpage, uint16_t new_x,
    uint16_t new_y);
static void M_MoveSprite(
    int index, RECTANGLE *old_bounds, uint16_t tpage, uint16_t new_x,
    uint16_t new_y);

static bool M_PackContainerAt(
    TEX_CONTAINER *container, TEX_PAGE *page, int x_pos, int y_pos);
static bool M_PackContainer(TEX_CONTAINER *container);

static PACKER_DATA *m_Data = NULL;
static int32_t m_StartPage = 0;
static int32_t m_EndPage = 0;
static int32_t m_UsedPageCount = 0;
static TEX_PAGE *m_VirtualPages = NULL;
static int32_t m_QueueSize = 0;
static TEX_CONTAINER *m_Queue = NULL;

bool Packer_Pack(PACKER_DATA *data)
{
    m_Data = data;

    m_StartPage = m_Data->level_page_count - 1;
    m_EndPage = MAX_TEXTPAGES - m_StartPage;
    m_UsedPageCount = 0;
    m_QueueSize = 0;

    M_AllocateNewPage();

    for (int i = 0; i < data->object_count; i++) {
        M_PrepareObject(i);
    }
    for (int i = 0; i < data->sprite_count; i++) {
        M_PrepareSprite(i);
    }

    bool result = true;
    for (int i = 0; i < m_QueueSize; i++) {
        TEX_CONTAINER *container = &m_Queue[i];
        if (!M_PackContainer(container)) {
            LOG_ERROR("Failed to pack container %d of %d", i, m_QueueSize);
            result = false;
            break;
        }
    }

    M_Cleanup();
    return result;
}

int32_t Packer_GetAddedPageCount(void)
{
    return m_UsedPageCount - 1;
}

static void M_PrepareObject(int object_index)
{
    const OBJECT_TEXTURE *const object_texture =
        &g_ObjectTextures[object_index];
    if (object_texture->tex_page == m_StartPage) {
        RECTANGLE *bounds = M_GetObjectBounds(object_texture);
        M_FillVirtualData(m_VirtualPages, bounds);
        Memory_FreePointer(&bounds);

    } else if (object_texture->tex_page > m_StartPage) {
        TEX_INFO *info = Memory_Alloc(sizeof(TEX_INFO));
        info->index = object_index;
        info->tpage = object_texture->tex_page;
        info->bounds = M_GetObjectBounds(object_texture);
        info->move = M_MoveObject;
        if (!M_EnqueueTexInfo(info)) {
            Memory_FreePointer(&info);
        }
    }
}

static void M_PrepareSprite(int sprite_index)
{
    const SPRITE_TEXTURE *const sprite_texture =
        &g_SpriteTextures[sprite_index];
    if (sprite_texture->tex_page == m_StartPage) {
        RECTANGLE *bounds = M_GetSpriteBounds(sprite_texture);
        M_FillVirtualData(m_VirtualPages, bounds);
        Memory_FreePointer(&bounds);
    } else if (sprite_texture->tex_page > m_StartPage) {
        TEX_INFO *info = Memory_Alloc(sizeof(TEX_INFO));
        info->index = sprite_index;
        info->tpage = sprite_texture->tex_page;
        info->bounds = M_GetSpriteBounds(sprite_texture);
        info->move = M_MoveSprite;
        if (!M_EnqueueTexInfo(info)) {
            Memory_FreePointer(&info);
        }
    }
}

static void M_FillVirtualData(TEX_PAGE *page, RECTANGLE *bounds)
{
    int y_end = bounds->y + bounds->h;
    int x_end = bounds->x + bounds->w;
    for (int y = bounds->y; y < y_end; y++) {
        for (int x = bounds->x; x < x_end; x++) {
            page->data[y * PAGE_WIDTH + x].r = 1;
            page->data[y * PAGE_WIDTH + x].g = 1;
            page->data[y * PAGE_WIDTH + x].b = 1;
            page->data[y * PAGE_WIDTH + x].a = 255;
        }
    }

    page->free_space -= bounds->w * bounds->h;
}

static bool M_EnqueueTexInfo(TEX_INFO *info)
{
    // This may be a child of another, so try to find its
    // parent first and add it there.
    if (m_Queue) {
        for (int i = 0; i < m_QueueSize; i++) {
            TEX_CONTAINER *container = &m_Queue[i];
            if (container->tex_infos->tpage != info->tpage) {
                continue;
            }

            RECTANGLE_COMPARISON comparison =
                M_Compare(container->bounds, info->bounds);
            if (comparison == RC_UNRELATED) {
                continue;
            }

            container->tex_infos = Memory_Realloc(
                container->tex_infos, sizeof(TEX_INFO) * (container->size + 1));
            container->tex_infos[container->size++] = *info;

            if (comparison == RC_COVERS) {
                // This is now the largest item in this container.
                container->bounds = info->bounds;
            }

            // Mark the item as no longer used and eligible for freeing.
            return false;
        }
    }

    // This doesn't have a parent, so make a new container.
    m_Queue =
        Memory_Realloc(m_Queue, sizeof(TEX_CONTAINER) * (m_QueueSize + 1));
    TEX_CONTAINER *new_container = &m_Queue[m_QueueSize++];
    new_container->size = 1;
    new_container->bounds = info->bounds;
    new_container->tex_infos = info;
    return true;
}

static RECTANGLE *M_GetObjectBounds(const OBJECT_TEXTURE *const texture)
{
    RECTANGLE *rectangle = Memory_Alloc(sizeof(RECTANGLE));

    int min_u = 0xFF, min_v = 0xFF;
    int max_u = 0, max_v = 0;

    for (int i = 0; i < 4; i++) {
        if (texture->uv[i].u == 0 && texture->uv[i].v == 0) {
            // This is a dummy vertex for a triangle.
            continue;
        }

        int u = (texture->uv[i].u & 0xFF00) >> 8;
        int v = (texture->uv[i].v & 0xFF00) >> 8;
        min_u = MIN(min_u, u);
        max_u = MAX(max_u, u);
        min_v = MIN(min_v, v);
        max_v = MAX(max_v, v);
    }

    rectangle->x = min_u;
    rectangle->y = min_v;
    rectangle->w = max_u - min_u + 1;
    rectangle->h = max_v - min_v + 1;

    return rectangle;
}

static RECTANGLE *M_GetSpriteBounds(const SPRITE_TEXTURE *const texture)
{
    RECTANGLE *rectangle = Memory_Alloc(sizeof(RECTANGLE));

    rectangle->x = texture->offset & 0xFF;
    rectangle->y = (texture->offset & 0xFF00) >> 8;
    rectangle->w = (texture->width + 1) / 256;
    rectangle->h = (texture->height + 1) / 256;

    return rectangle;
}

static bool M_PackContainer(TEX_CONTAINER *container)
{
    int size = container->bounds->w * container->bounds->h;
    if (size > PAGE_SIZE) {
        LOG_ERROR("Container is too large to pack");
        return false;
    }

    int y, x, y_end, x_end;
    for (int i = 0; i < m_EndPage; i++) {
        if (i == m_UsedPageCount) {
            M_AllocateNewPage();
        }

        TEX_PAGE *page = &m_VirtualPages[i];
        if (page->free_space < size) {
            continue;
        }

        y_end = PAGE_HEIGHT - container->bounds->h;
        x_end = PAGE_WIDTH - container->bounds->w;
        for (y = 0; y <= y_end; y++) {
            for (x = 0; x <= x_end; x++) {
                if (M_PackContainerAt(container, page, x, y)) {
                    return true;
                }
            }
        }
    }

    LOG_ERROR("Texture page limit reached");
    return false;
}

static void M_AllocateNewPage(void)
{
    m_VirtualPages = Memory_Realloc(
        m_VirtualPages, sizeof(TEX_PAGE) * (m_UsedPageCount + 1));
    TEX_PAGE *page = &m_VirtualPages[m_UsedPageCount];
    page->index = m_StartPage + m_UsedPageCount;
    page->free_space = PAGE_SIZE;
    memset(page->data, 0, PAGE_SIZE * sizeof(RGBA_8888));

    if (m_UsedPageCount > 0) {
        int new_count = m_Data->level_page_count + m_UsedPageCount;
        m_Data->level_pages = Memory_Realloc(
            m_Data->level_pages, PAGE_SIZE * new_count * sizeof(RGBA_8888));
        RGBA_8888 *level_page =
            m_Data->level_pages + (new_count - 1) * PAGE_SIZE;
        memset(level_page, 0, PAGE_SIZE * sizeof(RGBA_8888));
    }

    m_UsedPageCount++;
}

static bool M_PackContainerAt(
    TEX_CONTAINER *container, TEX_PAGE *page, int x_pos, int y_pos)
{
    int y_end = y_pos + container->bounds->h;
    int x_end = x_pos + container->bounds->w;

    for (int y = y_pos; y < y_end; y++) {
        for (int x = x_pos; x < x_end; x++) {
            if (page->data[y * PAGE_WIDTH + x].r
                || page->data[y * PAGE_WIDTH + x].g
                || page->data[y * PAGE_WIDTH + x].b
                || page->data[y * PAGE_WIDTH + x].a) {
                return false;
            }
        }
    }

    // There is adequate space at this position. Copy the pixel data from the
    // source texture page into the one identified, and fill the placeholder
    // data to avoid anything else taking this position.
    int source_page_index =
        container->tex_infos->tpage - m_Data->level_page_count;
    RGBA_8888 *source_page =
        m_Data->source_pages + source_page_index * PAGE_SIZE;
    RGBA_8888 *level_page = m_Data->level_pages + page->index * PAGE_SIZE;

    int old_pixel, new_pixel;
    for (int y = 0; y < container->bounds->h; y++) {
        for (int x = 0; x < container->bounds->w; x++) {
            old_pixel = (container->bounds->y + y) * PAGE_WIDTH
                + container->bounds->x + x;
            new_pixel = (y_pos + y) * PAGE_WIDTH + x_pos + x;
            page->data[new_pixel].r = 1;
            page->data[new_pixel].g = 1;
            page->data[new_pixel].b = 1;
            page->data[new_pixel].a = 255;
            level_page[new_pixel] = source_page[old_pixel];
        }
    }

    // Move each of the child tex_info coordinates accordingly.
    for (int i = 0; i < container->size; i++) {
        TEX_INFO *texture = &container->tex_infos[i];
        texture->move(
            texture->index, texture->bounds, page->index, x_pos, y_pos);
    }

    return true;
}

static void M_MoveObject(
    int index, RECTANGLE *old_bounds, uint16_t tpage, uint16_t new_x,
    uint16_t new_y)
{
    OBJECT_TEXTURE *const texture = &g_ObjectTextures[index];
    texture->tex_page = tpage;

    int x_diff = (new_x - old_bounds->x) << 8;
    int y_diff = (new_y - old_bounds->y) << 8;
    uint16_t u, v;
    for (int i = 0; i < 4; i++) {
        u = texture->uv[i].u;
        v = texture->uv[i].v;
        if (u == 0 && v == 0) {
            // This is a dummy vertex for a triangle.
            continue;
        }
        texture->uv[i].u = (x_diff + (u & 0xFF00)) | (u & 0xFF);
        texture->uv[i].v = (y_diff + (v & 0xFF00)) | (v & 0xFF);
    }
}

static void M_MoveSprite(
    int index, RECTANGLE *old_bounds, uint16_t tpage, uint16_t new_x,
    uint16_t new_y)
{
    SPRITE_TEXTURE *const texture = &g_SpriteTextures[index];
    texture->tex_page = tpage;
    texture->offset = (new_y << 8) | new_x;
}

static RECTANGLE_COMPARISON M_Compare(RECTANGLE *r1, RECTANGLE *r2)
{
    if (r1->x == r2->x && r1->w == r2->w && r1->y == r2->y && r1->h == r2->h) {
        return RC_EQUALS;
    }

    if (r1->x <= r2->x && r2->x + r2->w <= r1->x + r1->w && r1->y <= r2->y
        && r2->y + r2->h <= r1->y + r1->h) {
        return RC_CONTAINS;
    }

    if (r2->x <= r1->x && r1->x + r1->w <= r2->x + r2->w && r2->y <= r1->y
        && r1->y + r1->h <= r2->y + r2->h) {
        return RC_COVERS;
    }

    return RC_UNRELATED;
}

static void M_Cleanup(void)
{
    for (int i = 0; i < m_QueueSize; i++) {
        TEX_CONTAINER *container = &m_Queue[i];
        for (int j = 0; j < container->size; j++) {
            TEX_INFO *info = &container->tex_infos[j];
            Memory_FreePointer(&info->bounds);
        }
        Memory_FreePointer(&container->tex_infos);
    }

    Memory_FreePointer(&m_VirtualPages);
    Memory_FreePointer(&m_Queue);
}
