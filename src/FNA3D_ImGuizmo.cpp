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

#include <FNA3D.h>

#include <imgui.h>
#include <ImGuizmo.h>

extern "C" {

void FNA3D_ImGuizmo_BeginFrameEXT(FNA3D_Device *device)
{
	(void) device; /* device not needed (CPU-only), kept for API consistency */

	/* ImGuizmo::BeginFrame() creates an internal "gizmo" window (full
	 * viewport, NoInputs), captures its draw list, and resets per-frame
	 * state (mbOverGizmoHotspot).  This is the official entry point;
	 * callers do NOT need their own window for the gizmo. */
	ImGuizmo::BeginFrame();
}

void FNA3D_ImGuizmo_SetRectEXT(FNA3D_Device *device,
	float x, float y, float width, float height)
{
	(void) device;

	ImGuizmo::SetRect(x, y, width, height);
}

int FNA3D_ImGuizmo_ManipulateEXT(FNA3D_Device *device,
	const float *view, const float *projection,
	int operation, int mode, float *matrix)
{
	(void) device;

	return ImGuizmo::Manipulate(
		view, projection,
		static_cast<ImGuizmo::OPERATION>(operation),
		static_cast<ImGuizmo::MODE>(mode),
		matrix
	) ? 1 : 0;
}

int FNA3D_ImGuizmo_IsOverEXT(FNA3D_Device *device)
{
	(void) device;

	return ImGuizmo::IsOver() ? 1 : 0;
}

int FNA3D_ImGuizmo_IsUsingEXT(FNA3D_Device *device)
{
	(void) device;

	return ImGuizmo::IsUsing() ? 1 : 0;
}

} /* extern "C" */

/* vim: set noexpandtab shiftwidth=8 tabstop=8: */
