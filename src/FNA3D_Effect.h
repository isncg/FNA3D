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

#ifndef FNA3D_EFFECT_H
#define FNA3D_EFFECT_H

#include "FNA3D.h"

/* FEB (FNA3D Effect Binary) Format Constants */

#define FEB_MAGIC 0x42414E46 /* "FNAB" */
#define FEB_VERSION 1

/* Shader Stage */

typedef enum FNA3D_ShaderStage
{
	FNA3D_SHADERSTAGE_VERTEX,
	FNA3D_SHADERSTAGE_PIXEL,
} FNA3D_ShaderStage;

/* Render State Types (mirrors XNA/MojoShader render states) */

typedef enum FNA3D_EffectStateType
{
	FNA3D_EFFECTSTATE_ALPHABLENDENABLE,
	FNA3D_EFFECTSTATE_ALPHATESTENABLE,
	FNA3D_EFFECTSTATE_ALPHAFUNC,
	FNA3D_EFFECTSTATE_ALPHAREF,
	FNA3D_EFFECTSTATE_BLENDOP,
	FNA3D_EFFECTSTATE_COLORWRITEENABLE,
	FNA3D_EFFECTSTATE_CULLMODE,
	FNA3D_EFFECTSTATE_DEPTHBIAS,
	FNA3D_EFFECTSTATE_DESTBLEND,
	FNA3D_EFFECTSTATE_DITHERENABLE,
	FNA3D_EFFECTSTATE_FILLMODE,
	FNA3D_EFFECTSTATE_FOGENABLE,
	FNA3D_EFFECTSTATE_MULTISAMPLEALIAS,
	FNA3D_EFFECTSTATE_MULTISAMPLEMASK,
	FNA3D_EFFECTSTATE_SCISSORTESTENABLE,
	FNA3D_EFFECTSTATE_SLOPESCALEDEPTHBIAS,
	FNA3D_EFFECTSTATE_SRCBLEND,
	FNA3D_EFFECTSTATE_STENCILDEPTHBUFFERFAIL,
	FNA3D_EFFECTSTATE_STENCILENABLE,
	FNA3D_EFFECTSTATE_STENCILFAIL,
	FNA3D_EFFECTSTATE_STENCILFUNC,
	FNA3D_EFFECTSTATE_STENCILMASK,
	FNA3D_EFFECTSTATE_STENCILPASS,
	FNA3D_EFFECTSTATE_STENCILREF,
	FNA3D_EFFECTSTATE_STENCILWRITEMASK,
	FNA3D_EFFECTSTATE_ZENABLE,
	FNA3D_EFFECTSTATE_ZFUNC,
	FNA3D_EFFECTSTATE_ZWRITEENABLE,
} FNA3D_EffectStateType;

/* Sampler State Types */

typedef enum FNA3D_EffectSamplerStateType
{
	FNA3D_EFFECTSAMPLER_ADDRESSU,
	FNA3D_EFFECTSAMPLER_ADDRESSV,
	FNA3D_EFFECTSAMPLER_ADDRESSW,
	FNA3D_EFFECTSAMPLER_MAGFILTER,
	FNA3D_EFFECTSAMPLER_MINFILTER,
	FNA3D_EFFECTSAMPLER_MIPFILTER,
	FNA3D_EFFECTSAMPLER_MAXANISOTROPY,
	FNA3D_EFFECTSAMPLER_MAXMIPLEVEL,
	FNA3D_EFFECTSAMPLER_MIPMAPLODBIAS,
} FNA3D_EffectSamplerStateType;

/* Internal Effect Structures (defined before FNA3D_Effect which references them) */

/* Internal-only typedefs (public ones like FNA3D_Effect are in FNA3D.h) */
typedef struct FNA3D_EffectShader FNA3D_EffectShader;

struct FNA3D_EffectParam
{
	const char *name;
	const char *semantic;
	FNA3D_EffectParamType type;
	uint32_t registerIndex;
	union
	{
		float floatValues[16];  /* float/float2/float3/float4/matrix */
		int32_t intValues[16];
		uint32_t boolValue;
	} defaultValue;
	/* Runtime fields */
	uint8_t dirty;
	uint32_t bufferOffset;  /* byte offset in uniform data buffer */
	union
	{
		float floatValues[16];
		int32_t intValues[16];
		uint32_t boolValue;
	} currentValue;
};

/* Returns the byte size of an effect parameter type */
static inline uint32_t FNA3D_GetParamSize(FNA3D_EffectParamType type)
{
	switch (type)
	{
		case FNA3D_EFFECTPARAM_FLOAT:   return 4;
		case FNA3D_EFFECTPARAM_FLOAT2:  return 8;
		case FNA3D_EFFECTPARAM_FLOAT3:  return 12;
		case FNA3D_EFFECTPARAM_FLOAT4:  return 16;
		case FNA3D_EFFECTPARAM_INT:     return 4;
		case FNA3D_EFFECTPARAM_BOOL:    return 4;
		case FNA3D_EFFECTPARAM_MATRIX:  return 64;
		case FNA3D_EFFECTPARAM_TEXTURE:
		case FNA3D_EFFECTPARAM_TEXTURE1D:
		case FNA3D_EFFECTPARAM_TEXTURE2D:
		case FNA3D_EFFECTPARAM_TEXTURE3D:
		case FNA3D_EFFECTPARAM_TEXTURECUBE:
		default: return 0;
	}
}

struct FNA3D_EffectPass
{
	const char *name;
	int32_t vertexShaderIndex;  /* -1 if none */
	int32_t pixelShaderIndex;   /* -1 if none */
	/* State changes are stored inline */
	uint32_t renderStateCount;
	uint32_t samplerStateCount;
};

struct FNA3D_EffectTechnique
{
	const char *name;
	uint32_t passCount;
	FNA3D_EffectPass *passes;
};

struct FNA3D_EffectShader
{
	FNA3D_ShaderStage stage;
	const char *entryPoint;
	const uint8_t *spirvData;
	uint32_t spirvSize;
	uint32_t samplerCount;
	uint32_t uniformBufferCount;
};

struct FNA3D_EffectStateChanges
{
	uint32_t renderStateChangeCount;
	uint32_t samplerStateChangeCount;
	/* Data follows in allocated memory: first render states, then sampler states */
	/* This matches the layout expected by FNA's effect system */
	uint8_t data[];
};

struct FNA3D_Effect
{
	uint32_t techniqueCount;
	FNA3D_EffectTechnique *techniques;

	uint32_t passCount;
	FNA3D_EffectPass *passes;

	uint32_t paramCount;
	FNA3D_EffectParam *params;

	uint32_t shaderCount;
	FNA3D_EffectShader *shaders;

	/* State tracking for apply */
	const FNA3D_EffectTechnique *currentTechnique;
	uint32_t currentPass;

	/* Owned memory for the full FEB binary (contains strings + SPIR-V) */
	uint8_t *ownedData;
	uint32_t ownedDataSize;

	/* State changes buffer (pre-allocated for max pass size) */
	FNA3D_EffectStateChanges *stateChanges;

	/* Opaque pointer for driver-specific effect data (e.g. SDLGPU_Effect).
	 * Set by the driver during CreateEffect, read by driver during Apply etc.
	 */
	void *driverData;
};

/* Parser API (internal, used by effect loader and driver) */

uint8_t FNA3D_LoadEffect(
	const uint8_t *data,
	uint32_t dataLength,
	FNA3D_Effect **outEffect
);

void FNA3D_Internal_DestroyEffect(FNA3D_Effect *effect);

FNA3D_Effect* FNA3D_Internal_CloneEffect(const FNA3D_Effect *source);

/* Accessor helpers — implemented inline for performance.
 * Callers must have SDL headers (which provide NULL) included first.
 * Use these only from .c files that include SDL.h before this header.
 */

#endif /* FNA3D_EFFECT_H */

/* vim: set noexpandtab shiftwidth=8 tabstop=8: */
