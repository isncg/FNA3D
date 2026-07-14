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

#ifndef FNA3D_IMGUIZMO_H
#define FNA3D_IMGUIZMO_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* This header exposes an optional ImGuizmo integration for the FNA3D ImGui
 * SDL_GPU backend. ImGuizmo provides 3D gizmos (translate/rotate/scale) that
 * render into ImGui draw lists and can be used to manipulate object transforms
 * in 3D scenes.
 *
 * ImGuizmo is purely CPU-side (math + ImDrawList). It requires the ImGui
 * context to be active (FNA3D_ImGui_InitEXT must be called first).
 *
 * IMPORTANT: this integration is only available when FNA3D is built with the
 * FNA3D_IMGUIZMO CMake option enabled (default ON). When built without it,
 * these functions are safe no-ops.
 *
 * Typical per-frame usage:
 *   FNA3D_ImGui_InitEXT(device);                       // once, after CreateDevice
 *   ...
 *   FNA3D_ImGui_NewFrameEXT(device);                   // start of each frame
 *   FNA3D_ImGuizmo_BeginFrameEXT(device);               // after NewFrame
 *   FNA3D_ImGuizmo_SetRectEXT(device, 0, 0, w, h);      // set screen rect
 *   FNA3D_ImGuizmo_ManipulateEXT(device,                // apply gizmo
 *       view, proj, FNA3D_IMGUIZMO_TRANSLATE,
 *       FNA3D_IMGUIZMO_WORLD, matrix);
 *   ...
 *   FNA3D_SwapBuffers(...);                             // ImGui rendered here
 */

/* Gizmo operation constants (match ImGuizmo::OPERATION enum). */
#define FNA3D_IMGUIZMO_TRANSLATE_X  (1u << 0)
#define FNA3D_IMGUIZMO_TRANSLATE_Y  (1u << 1)
#define FNA3D_IMGUIZMO_TRANSLATE_Z  (1u << 2)
#define FNA3D_IMGUIZMO_ROTATE_X     (1u << 3)
#define FNA3D_IMGUIZMO_ROTATE_Y     (1u << 4)
#define FNA3D_IMGUIZMO_ROTATE_Z     (1u << 5)
#define FNA3D_IMGUIZMO_ROTATE_SCREEN (1u << 6)
#define FNA3D_IMGUIZMO_SCALE_X      (1u << 7)
#define FNA3D_IMGUIZMO_SCALE_Y      (1u << 8)
#define FNA3D_IMGUIZMO_SCALE_Z      (1u << 9)

#define FNA3D_IMGUIZMO_TRANSLATE    (FNA3D_IMGUIZMO_TRANSLATE_X | FNA3D_IMGUIZMO_TRANSLATE_Y | FNA3D_IMGUIZMO_TRANSLATE_Z)
#define FNA3D_IMGUIZMO_ROTATE       (FNA3D_IMGUIZMO_ROTATE_X | FNA3D_IMGUIZMO_ROTATE_Y | FNA3D_IMGUIZMO_ROTATE_Z | FNA3D_IMGUIZMO_ROTATE_SCREEN)
#define FNA3D_IMGUIZMO_SCALE        (FNA3D_IMGUIZMO_SCALE_X | FNA3D_IMGUIZMO_SCALE_Y | FNA3D_IMGUIZMO_SCALE_Z)

/* Gizmo mode constants (match ImGuizmo::MODE enum). */
#define FNA3D_IMGUIZMO_LOCAL  0
#define FNA3D_IMGUIZMO_WORLD  1

/* Begin a new ImGuizmo frame. Must be called after FNA3D_ImGui_NewFrameEXT
 * and before any other ImGuizmo function for the current frame.
 */
FNA3DAPI void FNA3D_ImGuizmo_BeginFrameEXT(FNA3D_Device *device);

/* Set the screen rectangle where the gizmo can be interacted with.
 * Typically set to cover the full rendering viewport.
 * Must be called before FNA3D_ImGuizmo_ManipulateEXT.
 */
FNA3DAPI void FNA3D_ImGuizmo_SetRectEXT(FNA3D_Device *device,
	float x, float y, float width, float height);

/* Draw and interact with a 3D gizmo that modifies the given 4x4 matrix.
 *
 * Parameters:
 *   view:       4x4 view matrix (column-major, float[16])
 *   projection: 4x4 projection matrix (column-major, float[16])
 *   operation:  which gizmo to show (FNA3D_IMGUIZMO_TRANSLATE, _ROTATE, _SCALE)
 *   mode:       FNA3D_IMGUIZMO_LOCAL or FNA3D_IMGUIZMO_WORLD
 *   matrix:     4x4 model matrix (column-major, float[16]), modified in place
 *
 * Returns 1 if the matrix was modified by user interaction this frame, 0
 * otherwise.
 */
FNA3DAPI int FNA3D_ImGuizmo_ManipulateEXT(FNA3D_Device *device,
	const float *view, const float *projection,
	int operation, int mode, float *matrix);

/* Returns 1 if the mouse cursor is over any gizmo control (axis, plane, or
 * screen component). Can be used to gate camera-control input.
 */
FNA3DAPI int FNA3D_ImGuizmo_IsOverEXT(FNA3D_Device *device);

/* Returns 1 if a gizmo is currently being dragged (IsOver or moving state). */
FNA3DAPI int FNA3D_ImGuizmo_IsUsingEXT(FNA3D_Device *device);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* FNA3D_IMGUIZMO_H */

/* vim: set noexpandtab shiftwidth=8 tabstop=8: */
