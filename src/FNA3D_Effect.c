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

#include "FNA3D_Effect.h"
#include "FNA3D_Driver.h"

#ifdef USE_SDL3
#include <SDL3/SDL.h>
#else
#include <SDL.h>
#endif

/* Binary Reader Helpers */

static inline uint32_t ReadU32(const uint8_t **buf)
{
	uint32_t val;
	SDL_memcpy(&val, *buf, sizeof(val));
	*buf += sizeof(val);
	return val;
}

static inline int32_t ReadI32(const uint8_t **buf)
{
	int32_t val;
	SDL_memcpy(&val, *buf, sizeof(val));
	*buf += sizeof(val);
	return val;
}

static inline uint8_t ReadU8(const uint8_t **buf)
{
	uint8_t val = **buf;
	*buf += 1;
	return val;
}

static inline const char* ResolveString(
	const uint8_t *stringTable,
	uint32_t offset,
	uint32_t stringTableSize
) {
	if (offset >= stringTableSize)
	{
		return NULL;
	}
	return (const char*) (stringTable + offset);
}

/* Effect Loading */

uint8_t FNA3D_LoadEffect(
	const uint8_t *data,
	uint32_t dataLength,
	FNA3D_Effect **outEffect
) {
	FNA3D_Effect *effect;
	const uint8_t *ptr = data;
	const uint8_t *stringTable;
	const uint8_t *paramData;
	const uint8_t *techniqueData;
	const uint8_t *passData;
	const uint8_t *shaderData;
	const uint8_t *spirvData;
	uint32_t magic, version;
	uint32_t techniqueCount, passCount, paramCount, shaderCount;
	uint32_t stringTableSize;
	uint32_t paramOffset, techniqueOffset, passOffset, shaderOffset, spirvOffset;
	uint32_t totalSize;
	uint32_t i, j;
	size_t allocSize;

	if (data == NULL || dataLength < 64 || outEffect == NULL)
	{
		return 0;
	}

	/* Read header */
	magic = ReadU32(&ptr);
	if (magic != FEB_MAGIC)
	{
		FNA3D_LogError("FEB: Invalid magic number!");
		return 0;
	}

	version = ReadU32(&ptr);
	if (version > FEB_VERSION)
	{
		FNA3D_LogError("FEB: Unsupported version %u!", version);
		return 0;
	}

	techniqueCount = ReadU32(&ptr);
	passCount = ReadU32(&ptr);
	paramCount = ReadU32(&ptr);
	shaderCount = ReadU32(&ptr);
	stringTableSize = ReadU32(&ptr);
	paramOffset = ReadU32(&ptr);
	techniqueOffset = ReadU32(&ptr);
	passOffset = ReadU32(&ptr);
	shaderOffset = ReadU32(&ptr);
	spirvOffset = ReadU32(&ptr);
	totalSize = ReadU32(&ptr);

	if (totalSize > dataLength)
	{
		FNA3D_LogError("FEB: Truncated file!");
		return 0;
	}

	/* Resolve section pointers */
	stringTable = data + 64; /* Header is 64 bytes: 16 * uint32_t */
	paramData = data + paramOffset;
	techniqueData = data + techniqueOffset;
	passData = data + passOffset;
	shaderData = data + shaderOffset;
	spirvData = data + spirvOffset;

	/* Bounds checks — parameter section uses variable-size entries,
	 * so we only check that the offset is within bounds (not stride*count). */
	if (paramOffset >= dataLength ||
		techniqueOffset + techniqueCount * 16 > dataLength ||
		passOffset + passCount * 24 > dataLength ||
		shaderOffset + shaderCount * 24 > dataLength ||
		spirvOffset > dataLength)
	{
		FNA3D_LogError("FEB: Section offsets out of bounds!");
		return 0;
	}

	/* Allocate effect */
	allocSize = sizeof(FNA3D_Effect)
		+ techniqueCount * sizeof(FNA3D_EffectTechnique)
		+ passCount * sizeof(FNA3D_EffectPass)
		+ paramCount * sizeof(FNA3D_EffectParam)
		+ shaderCount * sizeof(FNA3D_EffectShader);

	effect = (FNA3D_Effect*) SDL_calloc(1, allocSize);
	if (effect == NULL)
	{
		return 0;
	}

	effect->techniqueCount = techniqueCount;
	effect->passCount = passCount;
	effect->paramCount = paramCount;
	effect->shaderCount = shaderCount;
	effect->techniques = (FNA3D_EffectTechnique*) (effect + 1);
	effect->passes = (FNA3D_EffectPass*) (effect->techniques + techniqueCount);
	effect->params = (FNA3D_EffectParam*) (effect->passes + passCount);
	effect->shaders = (FNA3D_EffectShader*) (effect->params + paramCount);

	/* We take ownership of the data for strings and SPIR-V */
	effect->ownedDataSize = totalSize;
	effect->ownedData = (uint8_t*) SDL_malloc(totalSize);
	if (effect->ownedData == NULL)
	{
		SDL_free(effect);
		return 0;
	}
	SDL_memcpy(effect->ownedData, data, totalSize);

	/* Re-resolve pointers to the owned copy */
	stringTable = effect->ownedData + 64;
	paramData = effect->ownedData + paramOffset;
	techniqueData = effect->ownedData + techniqueOffset;
	passData = effect->ownedData + passOffset;
	shaderData = effect->ownedData + shaderOffset;
	spirvData = effect->ownedData + spirvOffset;

	/* Parse parameters — read sequentially, not with a fixed stride.
	 * Each parameter entry: nameOff(4) + semOff(4) + type(1) + pad(3)
	 * + registerIndex(4) + defaultValue(64) + annotationCount(4) = 84 bytes
	 * followed by annotationCount * 40 bytes of annotations.
	 */
	{
		uint32_t paramReadOff = 0;
		for (i = 0; i < paramCount; i++)
		{
			FNA3D_EffectParam *param = &effect->params[i];
			const uint8_t *p = paramData + paramReadOff;
			uint32_t nameOffset, semanticOffset;
			uint32_t annotationCount;
			uint8_t type;

			nameOffset = ReadU32(&p);
			semanticOffset = ReadU32(&p);
			type = ReadU8(&p);
			p += 3; /* padding */
			param->registerIndex = ReadU32(&p);
			param->type = (FNA3D_EffectParamType) type;
			param->name = ResolveString(stringTable, nameOffset, stringTableSize);
			param->semantic = ResolveString(stringTable, semanticOffset, stringTableSize);

			/* Read default value (16 floats = 64 bytes) */
			SDL_memcpy(&param->defaultValue, p, 64);
			p += 64;

			/* Skip annotations for now */
			annotationCount = ReadU32(&p);
			paramReadOff += 84 + annotationCount * 40;

			/* Initialize runtime fields */
			param->dirty = 0;
			param->bufferOffset = param->registerIndex * 16;
			SDL_memcpy(&param->currentValue, &param->defaultValue, 64);
		}
	}

	/* Parse techniques */
	for (i = 0; i < techniqueCount; i++)
	{
		FNA3D_EffectTechnique *technique = &effect->techniques[i];
		const uint8_t *p = techniqueData + i * 16;
		uint32_t nameOffset, passStart;
		uint32_t annotationCount;

		nameOffset = ReadU32(&p);
		passStart = ReadU32(&p);
		technique->passCount = ReadU32(&p);
		annotationCount = ReadU32(&p);
		technique->name = ResolveString(stringTable, nameOffset, stringTableSize);
		technique->passes = &effect->passes[passStart];

		/* Skip annotations for now (same format as param annotations) */
	}

	/* Parse passes */
	for (i = 0; i < passCount; i++)
	{
		FNA3D_EffectPass *pass = &effect->passes[i];
		const uint8_t *p = passData + i * 24;
		uint32_t nameOffset;

		nameOffset = ReadU32(&p);
		pass->vertexShaderIndex = ReadI32(&p);
		pass->pixelShaderIndex = ReadI32(&p);
		pass->renderStateCount = ReadU32(&p);
		pass->samplerStateCount = ReadU32(&p);
		p += 4; /* reserved */
		pass->name = ResolveString(stringTable, nameOffset, stringTableSize);
	}

	/* Parse shaders */
	for (i = 0; i < shaderCount; i++)
	{
		FNA3D_EffectShader *shader = &effect->shaders[i];
		const uint8_t *p = shaderData + i * 24;
		uint32_t entryOffset;
		uint32_t sOffset, sSize;

		shader->stage = (FNA3D_ShaderStage) ReadU8(&p);
		p += 3; /* padding */
		entryOffset = ReadU32(&p);
		sOffset = ReadU32(&p);
		sSize = ReadU32(&p);
		shader->samplerCount = ReadU32(&p);
		shader->uniformBufferCount = ReadU32(&p);

		shader->entryPoint = ResolveString(stringTable, entryOffset, stringTableSize);
		shader->spirvData = spirvData + sOffset;
		shader->spirvSize = sSize;
	}

	/* Allocate state changes buffer (worst-case size) */
	{
		uint32_t maxRenderStates = 0;
		uint32_t maxSamplerStates = 0;
		for (i = 0; i < passCount; i++)
		{
			if (effect->passes[i].renderStateCount > maxRenderStates)
				maxRenderStates = effect->passes[i].renderStateCount;
			if (effect->passes[i].samplerStateCount > maxSamplerStates)
				maxSamplerStates = effect->passes[i].samplerStateCount;
		}
		allocSize = sizeof(FNA3D_EffectStateChanges)
			+ maxRenderStates * 8   /* each render state: type(4) + value(4) */
			+ maxSamplerStates * 12; /* each sampler state: sampler(4) + type(4) + value(4) */
		effect->stateChanges = (FNA3D_EffectStateChanges*) SDL_calloc(1, allocSize);
	}

	*outEffect = effect;
	return 1;
}

void FNA3D_Internal_DestroyEffect(FNA3D_Effect *effect)
{
	if (effect == NULL)
	{
		return;
	}
	if (effect->stateChanges != NULL)
	{
		SDL_free(effect->stateChanges);
	}
	if (effect->ownedData != NULL)
	{
		SDL_free(effect->ownedData);
	}
	SDL_free(effect);
}

FNA3D_Effect* FNA3D_Internal_CloneEffect(const FNA3D_Effect *source)
{
	FNA3D_Effect *clone;

	if (source == NULL || source->ownedData == NULL)
	{
		return NULL;
	}

	/* Re-parse from the owned FEB binary data.
	 * This creates a fully independent copy with fresh internal arrays.
	 */
	if (!FNA3D_LoadEffect(source->ownedData, source->ownedDataSize, &clone))
	{
		return NULL;
	}

	return clone;
}

/* Public API accessors */

int32_t FNA3D_GetEffectTechniqueCount(FNA3D_Effect *effect)
{
	if (effect == NULL)
	{
		return 0;
	}
	return (int32_t) effect->techniqueCount;
}

FNA3D_EffectTechnique* FNA3D_GetEffectTechnique(
	FNA3D_Effect *effect,
	int32_t index
) {
	if (effect == NULL || index < 0 || (uint32_t) index >= effect->techniqueCount)
	{
		return NULL;
	}
	return &effect->techniques[index];
}

const char* FNA3D_GetTechniqueName(
	FNA3D_EffectTechnique *technique
) {
	if (technique == NULL)
	{
		return NULL;
	}
	return technique->name;
}

int32_t FNA3D_GetTechniquePassCount(
	FNA3D_EffectTechnique *technique
) {
	if (technique == NULL)
	{
		return 0;
	}
	return (int32_t) technique->passCount;
}

/* Parameter Introspection */

int32_t FNA3D_GetEffectParamCount(FNA3D_Effect *effect)
{
	if (effect == NULL)
	{
		return 0;
	}
	return (int32_t) effect->paramCount;
}

FNA3D_EffectParam* FNA3D_GetEffectParam(
	FNA3D_Effect *effect,
	int32_t index
) {
	if (effect == NULL || index < 0 || (uint32_t) index >= effect->paramCount)
	{
		return NULL;
	}
	return &effect->params[index];
}

FNA3D_EffectParam* FNA3D_GetEffectParamByName(
	FNA3D_Effect *effect,
	const char *name
) {
	uint32_t i;
	if (effect == NULL || name == NULL)
	{
		return NULL;
	}
	for (i = 0; i < effect->paramCount; i++)
	{
		if (SDL_strcmp(effect->params[i].name, name) == 0)
		{
			return &effect->params[i];
		}
	}
	return NULL;
}

const char* FNA3D_GetParamName(FNA3D_EffectParam *param)
{
	if (param == NULL)
	{
		return NULL;
	}
	return param->name;
}

const char* FNA3D_GetParamSemantic(FNA3D_EffectParam *param)
{
	if (param == NULL)
	{
		return NULL;
	}
	return param->semantic;
}

FNA3D_EffectParamType FNA3D_GetParamType(FNA3D_EffectParam *param)
{
	if (param == NULL)
	{
		return FNA3D_EFFECTPARAM_FLOAT;
	}
	return param->type;
}

uint32_t FNA3D_GetParamRegisterIndex(FNA3D_EffectParam *param)
{
	if (param == NULL)
	{
		return 0;
	}
	return param->registerIndex;
}

/* Pass Metadata */

const char* FNA3D_GetPassName(
	FNA3D_EffectTechnique *technique,
	int32_t passIndex
) {
	if (technique == NULL || passIndex < 0 || (uint32_t) passIndex >= technique->passCount)
	{
		return NULL;
	}
	return technique->passes[passIndex].name;
}

/* vim: set noexpandtab shiftwidth=8 tabstop=8: */
