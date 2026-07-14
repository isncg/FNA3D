# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

```bash
# Configure (Release, default)
cmake -B build -G Ninja . -DCMAKE_BUILD_TYPE=Release

# Build
ninja -C build

# Shared vs static library
cmake -B build -G Ninja . -DBUILD_SHARED_LIBS=OFF  # static

# Dear ImGui integration is ON by default; disable for a pure-C build
cmake -B build -G Ninja . -DFNA3D_IMGUI=OFF
```

There are no unit tests. Testing is done by building the library and running FNA-based games against it.

## Dear ImGui Integration (optional, `FNA3D_IMGUI`, default ON)

FNA3D can bundle Dear ImGui so the shared library's consumers (notably C# via
P/Invoke) get an ImGui overlay without touching SDL_GPU. Requires `python3` +
`ply` at build time (to run dear_bindings). Submodules live under `thirdparty/`:

- `thirdparty/imgui` — Dear ImGui core + `imgui_impl_sdl3` / `imgui_impl_sdlgpu3` backends.
- `thirdparty/dear_bindings` — generates `dcimgui.{h,cpp,json}` (flat C ABI) from `imgui.h`.

How it fits together (all C++ compiled directly into the FNA3D library):
- `dcimgui.cpp` exports the full ImGui widget API as `extern "C"` `ImGui_*`
  symbols (`CIMGUI_API`). This is what C# P/Invokes. `dcimgui.json` is installed
  so consumers can generate matching bindings (e.g. Hexa.NET.ImGui).
- `src/FNA3D_ImGui.cpp` is the only C++ TU authored here: `extern "C"`
  `FNA3D_INTERNAL_ImGui*` helpers that drive the SDL3/SDL_GPU backends using
  FNA3D's internal `SDL_GPUDevice`/window. An SDL event watch feeds input, so
  the host does not forward events.
- The driver renders ImGui inside `SDLGPU_SwapBuffers` (after the faux-backbuffer
  blit, LOAD render pass on the swapchain) and dispatches lifecycle through the
  vtable, exposed as `FNA3D_ImGui_*EXT` in the opt-in header
  `include/FNA3D_ImGui.h`. The existing `FNA3D.h` API is unchanged.

When `FNA3D_IMGUI=OFF` the library is pure C (no libstdc++, no ImGui symbols);
`FNA3D_ImGui_*EXT` remain as safe no-ops.

## Architecture

FNA3D is a C library implementing the XNA 4.0 Graphics API with SDL_GPU as its sole rendering backend. The library depends only on SDL 3.2.0+ and intentionally never uses the C runtime directly.

### Driver Dispatch Pattern

- `include/FNA3D.h` — Public API (all functions take `FNA3D_Device*`).
- `src/FNA3D_Driver.h` — Defines `FNA3D_Device` as a vtable of function pointers plus an opaque `driverData` pointer. The `ASSIGN_DRIVER(name)` macro fills every slot from `name##_FunctionName`.
- `src/FNA3D.c` — Public API dispatch. Every function is a thin wrapper that calls through `device->FunctionName(device->driverData, ...)`. The `drivers[]` array contains only `SDLGPUDriver`.

### Single Backend: SDL GPU

`src/FNA3D_Driver_SDL.c` (~4500 lines) is the sole rendering backend. All graphics operations go through the SDL_GPU API (`SDL_CreateGPUShader`, `SDL_BindGPUGraphicsPipeline`, `SDL_DrawGPUIndexedPrimitives`, etc.).

### Effect and Shader System

`src/FNA3D_Effect.c` and `src/FNA3D_Effect.h` implement the FNA3D Effect Binary (FEB) format — a custom binary format bundling effect metadata (techniques, passes, parameters, render states) with pre-compiled SPIR-V shader binaries.

Shader pipeline: **HLSL source → DXC (SPIR-V backend) → FEB binary (SPIR-V + metadata)**. At runtime, FNA3D loads SPIR-V from the FEB and feeds it directly to `SDL_CreateGPUShader` (`SDL_GPU_SHADERFORMAT_SPIRV`). There is no runtime cross-compilation: device creation requests only the SPIR-V shader format, which restricts SDL_GPU to its Vulkan driver.

The effect binary is self-contained — all strings, metadata, and SPIR-V data are in a single blob with section offsets.

Key types (opaque in public API, defined in `src/FNA3D_Effect.h`):
- `FNA3D_Effect` — parsed effect with arrays of techniques, passes, params, shaders
- `FNA3D_EffectTechnique` — named technique with pass list
- `FNA3D_EffectPass` — pass with vertex/pixel shader indices, render states
- `FNA3D_EffectShader` — SPIR-V binary + entry point for a shader stage
- `FNA3D_EffectStateChanges` — state change buffer filled during effect application

### Pipeline Cache (`src/FNA3D_PipelineCache.c`)

State hashing and caching subsystem. Packs graphics pipeline states (blend, depth-stencil, rasterizer, sampler) into `uint64_t` pairs for fast lookup. Used by the SDL GPU driver to cache compiled pipeline objects and vertex buffer binding layouts.

### Image Loading (`src/FNA3D_Image.c`)

Separate public API (`include/FNA3D_Image.h`) for decoding PNG/JPG/GIF into RGBA8 and encoding PNG/JPG. Uses vendored `stb_image.h` and `stb_image_write.h` in `src/`. Stream-based callbacks rather than direct file I/O.

## Code Conventions

- **C dialect**: `-std=gnu99` with `-Wall -Wno-strict-aliasing -pedantic`
- **Formatting**: Tabs (tabstop=8), no spaces for indentation (see `vim: set noexpandtab shiftwidth=8 tabstop=8:` at file bottoms)
- **Naming**: `FNA3D_` prefix for all public types/functions. `FNA3DAPI`/`FNA3DCALL` decorators for public API visibility
- **Memory**: Never calls `malloc`/`free` directly — uses `SDL_malloc`/`SDL_free`/`SDL_calloc`
- **Logging**: `FNA3D_LogInfo`/`FNA3D_LogWarn`/`FNA3D_LogError` route through SDL's logging system
- **Resource disposal**: Functions named `AddDispose*` (not `Destroy*`) because disposal may be deferred from the rendering thread
- **Version**: `FNA3D_ABI_VERSION=1`, `FNA3D_MAJOR_VERSION=27` (in both `CMakeLists.txt` and `include/FNA3D.h`)

## Key Removals (hlsl branch)

The following were removed from the codebase:
- **MojoShader** — git submodule entirely removed. Shader compilation is now HLSL→DXC→SPIR-V (content pipeline); SPIR-V is consumed natively at runtime (Vulkan only, no cross-compilation)
- **D3D11 backend** — `src/FNA3D_Driver_D3D11.*` deleted
- **OpenGL backend** — `src/FNA3D_Driver_OpenGL.*` deleted
- **Tracing subsystem** — `src/FNA3D_Tracing.*`, `replay/`, `dumpspirv/` deleted
- **Platform projects** — `visualc/`, `visualc-gdk/`, `Xcode/` deleted

## FEB Binary Format

The FNA3D Effect Binary (FEB) is defined by `FEB_MAGIC = 0x42414E46` ("FNAB") in `src/FNA3D_Effect.h`. Version 1 layout:

```
[Header: 64 bytes] magic + version + counts + section offsets
[String Table]     null-terminated strings
[Parameters]       name, type, register, default value
[Techniques]       name, pass range
[Passes]           name, shader indices, render/sampler state counts
[Shaders]          stage, entry point, SPIR-V offset/size
[SPIR-V Data]      raw SPIR-V binaries
```

## Git

- Main branch: `master` (upstream at `FNA-XNA/FNA3D`)
- Current branch: `hlsl` — fork at `isncg/FNA3D` for HLSL-only, SDL_GPU-only refactoring
- Commit style: `Subsystem: Brief description`
