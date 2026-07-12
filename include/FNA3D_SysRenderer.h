/* FNA3D - 3D Graphics Library for FNA
 *
 * Copyright (c) 2020-2024 Ethan Lee
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * In no event will the authors be held liable for any damages arising from
 * the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 * claim that you wrote the original software. If you use this software in a
 * product, an acknowledgment in the product documentation would be
 * appreciated but is not required.
 *
 * 2. Altered source versions must be plainly marked as such, and must not be
 * misrepresented as being the original software.
 *
 * 3. This notice may not be removed or altered from any source distribution.
 *
 * Ethan "flibitijibibo" Lee <flibitijibibo@flibitijibibo.com>
 *
 */

#ifndef FNA3D_SYSRENDERER_H
#define FNA3D_SYSRENDERER_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* This header exposes an in-progress extension to access the rendering context
 * of the active FNA3D backend, as well as push texture objects to the backend.
 *
 * In general you do NOT want to use anything in here, in fact this whole
 * extension is skipped unless you explicitly include the header in your
 * application.
 */

#define FNA3D_SYSRENDERER_VERSION_EXT 0

typedef enum FNA3D_SysRendererTypeEXT
{
	FNA3D_RENDERER_TYPE_SDL_GPU_EXT,
} FNA3D_SysRendererTypeEXT;

typedef struct FNA3D_SysRendererEXT
{
	uint32_t version;
	FNA3D_SysRendererTypeEXT rendererType;

	union
	{
		uint8_t filler[64];
	} renderer;
} FNA3D_SysRendererEXT;

typedef struct FNA3D_SysTextureEXT
{
	uint32_t version;
	FNA3D_SysRendererTypeEXT rendererType;

	union
	{
		uint8_t filler[64];
	} texture;
} FNA3D_SysTextureEXT;

/* Export the internal rendering context. */
FNA3DAPI void FNA3D_GetSysRendererEXT(
	FNA3D_Device *device,
	FNA3D_SysRendererEXT *sysrenderer
);

/* Import a texture reference that is marked as internal */
FNA3DAPI FNA3D_Texture* FNA3D_CreateSysTextureEXT(
	FNA3D_Device *device,
	FNA3D_SysTextureEXT *systexture
);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* FNA3D_SYSRENDERER_H */

/* vim: set noexpandtab shiftwidth=8 tabstop=8: */
