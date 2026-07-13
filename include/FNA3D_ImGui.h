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

#ifndef FNA3D_IMGUI_H
#define FNA3D_IMGUI_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* This header exposes an optional Dear ImGui integration for the FNA3D
 * SDL_GPU backend. Like FNA3D_SysRenderer.h, the whole extension is skipped
 * unless you explicitly include this header in your application.
 *
 * These functions manage ONLY the Dear ImGui backend lifecycle (context,
 * SDL3 platform backend and SDL_GPU renderer backend). FNA3D owns the
 * underlying SDL_GPUDevice and window, so you never touch SDL_GPU directly.
 * The actual ImGui widget API (ImGui_Begin, ImGui_Text, ImGui_Button, ...) is
 * provided by the dcimgui C bindings exported from this same library, intended
 * to be consumed from C# via P/Invoke.
 *
 * IMPORTANT: this integration is only available when FNA3D is built with the
 * FNA3D_IMGUI CMake option enabled (default ON). When built without it, these
 * functions are safe no-ops.
 *
 * Typical per-frame usage (from the host, e.g. C#):
 *   FNA3D_ImGui_InitEXT(device);        // once, after FNA3D_CreateDevice
 *   ...
 *   FNA3D_ImGui_NewFrameEXT(device);    // start of each frame
 *   ImGui_ShowDemoWindow(...);          // build UI via dcimgui
 *   FNA3D_SwapBuffers(...);             // ImGui is drawn automatically here
 *   ...
 *   FNA3D_ImGui_ShutdownEXT(device);    // on teardown
 *
 * Input is captured automatically via an SDL event watch installed at Init,
 * so you do not normally need to forward events. FNA3D_ImGui_ProcessEventEXT
 * is provided for advanced/manual event pumping only.
 */

/* Initialize the Dear ImGui context and SDL3/SDL_GPU backends. Call once,
 * after FNA3D_CreateDevice. Installs an SDL event watch for input capture.
 */
FNA3DAPI void FNA3D_ImGui_InitEXT(FNA3D_Device *device);

/* Begin a new Dear ImGui frame. Call once at the start of each frame, before
 * issuing any ImGui widget calls.
 */
FNA3DAPI void FNA3D_ImGui_NewFrameEXT(FNA3D_Device *device);

/* (Advanced) Manually forward an SDL_Event to the ImGui SDL3 backend. Pass a
 * pointer to an SDL_Event. Returns 1 if ImGui wants to consume the event.
 * Not needed when relying on the built-in event watch installed by Init.
 */
FNA3DAPI uint8_t FNA3D_ImGui_ProcessEventEXT(FNA3D_Device *device, void *sdlEvent);

/* Shut down the Dear ImGui backends and destroy the context. */
FNA3DAPI void FNA3D_ImGui_ShutdownEXT(FNA3D_Device *device);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* FNA3D_IMGUI_H */

/* vim: set noexpandtab shiftwidth=8 tabstop=8: */
