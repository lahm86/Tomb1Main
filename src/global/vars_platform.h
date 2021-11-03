#ifndef T1M_GLOBAL_VARS_PLATFORM_H
#define T1M_GLOBAL_VARS_PLATFORM_H

#include "inject_util.h"
#include "specific/ati.h"

#include <ddraw.h>
#include <dsound.h>
#include <windows.h>

// clang-format off
#define TombHWND                VAR_U_(0x00463600, HWND)
#define AuxDeviceID             VAR_U_(0x0045B984, uint32_t)
#define MCIDeviceID             VAR_U_(0x0045B994, uint32_t)
#define DSound                  VAR_U_(0x0045F1CC, LPDIRECTSOUND)
#define HATI3DCIFModule         VAR_U_(0x00459CF0, HMODULE)
#define Surface1                VAR_U_(0x005DA6A4, LPDIRECTDRAWSURFACE)
#define Surface2                VAR_U_(0x005DB484, LPDIRECTDRAWSURFACE)
#define Surface3                VAR_U_(0x005DA744, LPDIRECTDRAWSURFACE)
#define Surface4                VAR_U_(0x00463608, LPDIRECTDRAWSURFACE)
#define TextureSurfaces         ARRAY_(0x005DA6C0, LPDIRECTDRAWSURFACE, [MAX_TEXTPAGES])
#define Surface1DrawPtr         VAR_U_(0x00463564, void*)
#define Surface2DrawPtr         VAR_U_(0x005DB480, void*)
#define DDraw                   VAR_U_(0x0045A998, LPDIRECTDRAW)
#define ATIRenderContext        VAR_U_(0x0045A994, C3D_HRC)
#define ATIInfo                 VAR_U_(0x0045A960, C3D_3DCIFINFO)
#define ATITextureMap           ARRAY_(0x00463580, C3D_HTX, [MAX_TEXTPAGES])
#define ATITexturePalette       VAR_U_(0x005DA7E0, C3D_HTXPAL)
#define ATIPalette              ARRAY_(0x00462DC0, C3D_PALETTENTRY, [256])
#define ATIChromaKey            VAR_U_(0x00463614, C3D_COLOR)
// clang-format on

extern HINSTANCE TombModule;
extern float DDrawSurfaceMinX;
extern float DDrawSurfaceMinY;
extern float DDrawSurfaceMaxX;
extern float DDrawSurfaceMaxY;
extern int32_t DDrawSurfaceWidth;
extern int32_t DDrawSurfaceHeight;

#endif
