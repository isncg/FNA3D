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

/* Dear ImGui glue for the FNA3D SDL_GPU backend.
 *
 * This is the ONLY C++ translation unit in FNA3D. It is compiled (together
 * with the Dear ImGui core, its SDL3/SDL_GPU backends, and the generated
 * dcimgui C bindings) only when the FNA3D_IMGUI CMake option is enabled.
 *
 * It exposes a handful of extern "C" entry points that the (C) SDL_GPU driver
 * calls into. All Dear ImGui C++ state lives here; the driver never sees a C++
 * symbol. FNA3D owns the SDL_GPUDevice / SDL_Window, so the host (e.g. C#)
 * never touches SDL_GPU directly.
 */

#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlgpu3.h"

#include <SDL3/SDL.h>

#include <stdint.h>

/* Is a Dear ImGui frame currently open (NewFrame called, not yet Rendered)? */
static bool FNA3D_ImGui_frameActive = false;

/* Forward SDL events into the ImGui SDL3 backend. Installed as an SDL event
 * watch so the host does not have to plumb events through manually.
 */
static bool SDLCALL FNA3D_ImGui_EventWatch(void *userdata, SDL_Event *event)
{
	(void) userdata;
	if (ImGui::GetCurrentContext() != NULL)
	{
		ImGui_ImplSDL3_ProcessEvent(event);
	}
	/* Return value is ignored for watchers, but must be a bool. */
	return true;
}

extern "C" {

void FNA3D_INTERNAL_ImGuiInit(
	SDL_GPUDevice *device,
	SDL_Window *window,
	SDL_GPUTextureFormat swapchainFormat
) {
	if (ImGui::GetCurrentContext() != NULL)
	{
		/* Already initialized. */
		return;
	}

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGuiIO &io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

	ImGui::StyleColorsDark();

	ImGui_ImplSDL3_InitForSDLGPU(window);

	ImGui_ImplSDLGPU3_InitInfo initInfo;
	SDL_memset(&initInfo, '\0', sizeof(initInfo));
	initInfo.Device = device;
	initInfo.ColorTargetFormat = swapchainFormat;
	initInfo.MSAASamples = SDL_GPU_SAMPLECOUNT_1;
	ImGui_ImplSDLGPU3_Init(&initInfo);

	SDL_AddEventWatch(FNA3D_ImGui_EventWatch, NULL);

	FNA3D_ImGui_frameActive = false;
}

void FNA3D_INTERNAL_ImGuiNewFrame(void)
{
	if (ImGui::GetCurrentContext() == NULL)
	{
		return;
	}

	/* If the previous frame was never presented (e.g. the swapchain texture
	 * could not be acquired), discard it so ImGui::NewFrame() doesn't assert.
	 */
	if (FNA3D_ImGui_frameActive)
	{
		ImGui::EndFrame();
		FNA3D_ImGui_frameActive = false;
	}

	ImGui_ImplSDLGPU3_NewFrame();
	ImGui_ImplSDL3_NewFrame();
	ImGui::NewFrame();
	FNA3D_ImGui_frameActive = true;
}

uint8_t FNA3D_INTERNAL_ImGuiProcessEvent(void *sdlEvent)
{
	if (ImGui::GetCurrentContext() == NULL)
	{
		return 0;
	}
	return ImGui_ImplSDL3_ProcessEvent((const SDL_Event*) sdlEvent) ? 1 : 0;
}

void FNA3D_INTERNAL_ImGuiRender(
	SDL_GPUCommandBuffer *commandBuffer,
	SDL_GPUTexture *swapchainTexture,
	uint32_t width,
	uint32_t height
) {
	ImDrawData *drawData;
	SDL_GPUColorTargetInfo targetInfo;
	SDL_GPURenderPass *renderPass;

	(void) width;
	(void) height;

	if (	ImGui::GetCurrentContext() == NULL ||
		!FNA3D_ImGui_frameActive	)
	{
		return;
	}

	ImGui::Render();
	FNA3D_ImGui_frameActive = false;

	drawData = ImGui::GetDrawData();
	if (	drawData == NULL ||
		drawData->DisplaySize.x <= 0.0f ||
		drawData->DisplaySize.y <= 0.0f	)
	{
		return;
	}

	/* MANDATORY: upload vertex/index buffers BEFORE beginning the render pass.
	 * SDL_GPU forbids copy operations during an active render pass.
	 */
	ImGui_ImplSDLGPU3_PrepareDrawData(drawData, commandBuffer);

	/* Render over the already-blitted scene: LOAD to preserve it. */
	SDL_memset(&targetInfo, '\0', sizeof(targetInfo));
	targetInfo.texture = swapchainTexture;
	targetInfo.load_op = SDL_GPU_LOADOP_LOAD;
	targetInfo.store_op = SDL_GPU_STOREOP_STORE;
	targetInfo.mip_level = 0;
	targetInfo.layer_or_depth_plane = 0;
	targetInfo.cycle = false;

	renderPass = SDL_BeginGPURenderPass(commandBuffer, &targetInfo, 1, NULL);
	ImGui_ImplSDLGPU3_RenderDrawData(drawData, commandBuffer, renderPass);
	SDL_EndGPURenderPass(renderPass);
}

void FNA3D_INTERNAL_ImGuiShutdown(void)
{
	if (ImGui::GetCurrentContext() == NULL)
	{
		return;
	}

	SDL_RemoveEventWatch(FNA3D_ImGui_EventWatch, NULL);

	ImGui_ImplSDLGPU3_Shutdown();
	ImGui_ImplSDL3_Shutdown();
	ImGui::DestroyContext();

	FNA3D_ImGui_frameActive = false;
}

} /* extern "C" */

/* vim: set noexpandtab shiftwidth=8 tabstop=8: */
