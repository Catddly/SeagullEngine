#include "IRenderer.h"

#include <include/SpirvTools.h>

#include "Interface/ILog.h"
#include "Interface/IMemory.h"

#ifdef SG_GRAPHIC_API_VULKAN

namespace SG
{

	static DescriptorType SPIRV_TO_DESCRIPTOR[SPIRV_TYPE_COUNT] = {
		SG_DESCRIPTOR_TYPE_UNDEFINED,   SG_DESCRIPTOR_TYPE_UNDEFINED,
		SG_DESCRIPTOR_TYPE_UNIFORM_BUFFER,  SG_DESCRIPTOR_TYPE_RW_BUFFER,
		SG_DESCRIPTOR_TYPE_TEXTURE, SG_DESCRIPTOR_TYPE_RW_TEXTURE, 
		SG_DESCRIPTOR_TYPE_SAMPLER, SG_DESCRIPTOR_TYPE_ROOT_CONSTANT,
		SG_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, SG_DESCRIPTOR_TYPE_TEXEL_BUFFER, 
		SG_DESCRIPTOR_TYPE_RW_TEXEL_BUFFER, SG_DESCRIPTOR_TYPE_RAY_TRACING,
		SG_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
	};

	static TextureDimension SPIRV_TO_RESOURCE_DIM[SPIRV_DIM_COUNT] = {
		SG_TEXTURE_DIM_UNDEFINED,
		SG_TEXTURE_DIM_UNDEFINED,
		SG_TEXTURE_DIM_1D,
		SG_TEXTURE_DIM_1D_ARRAY,
		SG_TEXTURE_DIM_2D,
		SG_TEXTURE_DIM_2D_ARRAY,
		SG_TEXTURE_DIM_2DMS,
		SG_TEXTURE_DIM_2DMS_ARRAY,
		SG_TEXTURE_DIM_3D,
		SG_TEXTURE_DIM_CUBE,
		SG_TEXTURE_DIM_CUBE_ARRAY,
	};

	bool filter_resouce(SpirvResource* resource, ShaderStage currentStage)
	{
		bool filter = false;

		// remove used resouces
		// TODO: log warning
		filter = filter || (resource->isUsed == false);

		// remove stage outputs
		filter = filter || (resource->type == SpirvResourceType::SPIRV_TYPE_STAGE_OUTPUTS);

		// remove stage inputs that are not on the vertex shader
		filter = filter || (resource->type == SpirvResourceType::SPIRV_TYPE_STAGE_INPUTS && currentStage != SG_SHADER_STAGE_VERT);

		return filter;
	}

	void vk_create_shader_reflection(const uint8_t* shaderCode, uint32_t shaderSize, ShaderStage shaderStage, ShaderReflection* pOutReflection)
	{
		if (pOutReflection == nullptr)
		{
			SG_LOG_ERROR("Create Shader Refection failed. Invalid reflection output!");
			return;    // TODO: error msg
		}

		CrossCompiler crossComplier;

		create_cross_complier((const uint32_t*)shaderCode, shaderSize / sizeof(uint32_t), &crossComplier);

		reflect_entry_point(&crossComplier);
		reflect_shader_resources(&crossComplier);
		reflect_shader_variables(&crossComplier);

		if (shaderStage == SG_SHADER_STAGE_COMP)
		{
			reflect_compute_shader_work_groups_size(
				&crossComplier, &pOutReflection->numThreadsPerGroup[0], &pOutReflection->numThreadsPerGroup[1], &pOutReflection->numThreadsPerGroup[2]);
		}
		else if (shaderStage == SG_SHADER_STAGE_TESC)
		{
			reflect_hull_shader_control_point(&crossComplier, &pOutReflection->numControlPoint);
		}

		// lets find out the size of the name pool we need
		// also get number of resources while we are at it
		uint32_t namePoolSize = 0;
		uint32_t vertexInputCount = 0;
		uint32_t resouceCount = 0;
		uint32_t variablesCount = 0;

		namePoolSize += crossComplier.entryPointSize + 1;

		for (uint32_t i = 0; i < crossComplier.ShaderResourceCount; ++i)
		{
			SpirvResource* resource = crossComplier.pShaderResouces + i;

			// filter out what we don't use
			if (!filter_resouce(resource, shaderStage))
			{
				namePoolSize += resource->nameSize + 1;

				if (resource->type == SpirvResourceType::SPIRV_TYPE_STAGE_INPUTS && shaderStage == SG_SHADER_STAGE_VERT)
				{
					++vertexInputCount;
				}
				else
				{
					++resouceCount;
				}
			}
		}

		for (uint32_t i = 0; i < crossComplier.uniformVariablesCount; ++i)
		{
			SpirvVariable* variable = crossComplier.pUniformVariables + i;

			// check if parent buffer was filtered out
			bool parentFiltered = filter_resouce(crossComplier.pShaderResouces + variable->parentIndex, shaderStage);

			// filter out what we don't use
			// TODO: log warning
			if (variable->isUsed && !parentFiltered)
			{
				namePoolSize += variable->nameSize + 1;
				++variablesCount;
			}
		}

		// we now have the size of the memory pool and number of resources
		char* namePool = NULL;
		if (namePoolSize)
			namePool = (char*)sg_calloc(namePoolSize, 1);
		char* pCurrentName = namePool;

		pOutReflection->pEntryPoint = pCurrentName;
		memcpy(pCurrentName, crossComplier.pEntryPoint, crossComplier.entryPointSize);
		pCurrentName += crossComplier.entryPointSize + 1;

		VertexInput* pVertexInputs = nullptr;

		// start with the vertex input
		if (shaderStage == SG_SHADER_STAGE_VERT && vertexInputCount > 0)
		{
			pVertexInputs = (VertexInput*)sg_malloc(sizeof(VertexInput) * vertexInputCount);

			uint32_t j = 0;
			for (uint32_t i = 0; i < crossComplier.ShaderResourceCount; ++i)
			{
				SpirvResource* resource = crossComplier.pShaderResouces + i;

				// filter out what we don't use
				if (!filter_resouce(resource, shaderStage) && resource->type == SpirvResourceType::SPIRV_TYPE_STAGE_INPUTS)
				{
					pVertexInputs[j].size = resource->size;
					pVertexInputs[j].name = pCurrentName;
					pVertexInputs[j].nameSize = resource->nameSize;
					// we don't own the names memory we need to copy it to the name pool
					memcpy(pCurrentName, resource->name, resource->nameSize);
					pCurrentName += resource->nameSize + 1;
					++j;
				}
			}
		}

		uint32_t* indexRemap = nullptr;
		ShaderResource* pResources = nullptr;
		// continue with resources
		if (resouceCount)
		{
			indexRemap = (uint32_t*)sg_malloc(sizeof(uint32_t) * crossComplier.ShaderResourceCount);
			pResources = (ShaderResource*)sg_malloc(sizeof(ShaderResource) * resouceCount);

			uint32_t j = 0;
			for (uint32_t i = 0; i < crossComplier.ShaderResourceCount; ++i)
			{
				// set index remap
				indexRemap[i] = (uint32_t)-1;

				SpirvResource* resource = crossComplier.pShaderResouces + i;

				// filter out what we don't use
				if (!filter_resouce(resource, shaderStage) && resource->type != SpirvResourceType::SPIRV_TYPE_STAGE_INPUTS)
				{
					// set new index
					indexRemap[i] = j;

					pResources[j].type = SPIRV_TO_DESCRIPTOR[resource->type];
					pResources[j].set = resource->set;
					pResources[j].reg = resource->binding;
					pResources[j].size = resource->size;
					pResources[j].usedStages = shaderStage;

					pResources[j].name = pCurrentName;
					pResources[j].nameSize = resource->nameSize;
					pResources[j].dim = SPIRV_TO_RESOURCE_DIM[resource->dim];
					// we don't own the names memory we need to copy it to the name pool
					memcpy(pCurrentName, resource->name, resource->nameSize);
					pCurrentName += resource->nameSize + 1;
					++j;
				}
			}
		}

		ShaderVariable* pVariables = nullptr;
		// now do variables
		if (variablesCount)
		{
			pVariables = (ShaderVariable*)sg_malloc(sizeof(ShaderVariable) * variablesCount);

			uint32_t j = 0;
			for (uint32_t i = 0; i < crossComplier.uniformVariablesCount; ++i)
			{
				SpirvVariable* variable = crossComplier.pUniformVariables + i;

				// check if parent buffer was filtered out
				bool parentFiltered = filter_resouce(crossComplier.pShaderResouces + variable->parentIndex, shaderStage);

				// filter out what we don't use
				if (variable->isUsed && !parentFiltered)
				{
					pVariables[j].offset = variable->offset;
					pVariables[j].size = variable->size;
					pVariables[j].parentIndex = indexRemap[variable->parentIndex];

					pVariables[j].name = pCurrentName;
					pVariables[j].nameSize = variable->nameSize;
					// we don't own the names memory we need to copy it to the name pool
					memcpy(pCurrentName, variable->name, variable->nameSize);
					pCurrentName += variable->nameSize + 1;
					++j;
				}
			}
		}

		sg_free(indexRemap);
		destroy_cross_complier(&crossComplier);

		// all refection structs should be built now
		pOutReflection->shaderStage = shaderStage;

		pOutReflection->pNamePool = namePool;
		pOutReflection->namePoolSize = namePoolSize;

		pOutReflection->pVertexInputs = pVertexInputs;
		pOutReflection->vertexInputsCount = vertexInputCount;

		pOutReflection->pShaderResources = pResources;
		pOutReflection->shaderResourceCount = resouceCount;

		pOutReflection->pVariables = pVariables;
		pOutReflection->variableCount = variablesCount;
	}
}

#endif // SG_GRAPHIC_API_VULKAN