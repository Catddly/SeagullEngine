#ifdef SG_GRAPHIC_API_VULKAN
#include <vulkan/vulkan_core.h>
#endif

#include "IRenderer.h"

#include "Interface/ILog.h"
#include "Interface/IMemory.h"

namespace SG
{

#define SG_RESOURCE_NAME_CHECK

	static bool shader_resource_compare(ShaderResource* a, ShaderResource* b)
	{
		bool isSame = true;

		isSame = isSame && (a->type == b->type);
		isSame = isSame && (a->set == b->set);
		isSame = isSame && (a->reg == b->reg);

#ifdef SG_GRAPHIC_API_METAL
		isSame = isSame && (a->mtlArgumentDescriptors.argumentIndex == b->mtlArgumentDescriptors.argumentIndex);
#endif

#ifdef SG_RESOURCE_NAME_CHECK
		// we may not need this, the rest is enough but if we want to be super sure we can do this check
		isSame = isSame && (a->nameSize == b->nameSize);
		// early exit before string cmp
		if (isSame == false)
			return isSame;

		isSame = (strcmp(a->name, b->name) == 0);
#endif
		return isSame;
	}

	static bool shader_variable_compare(ShaderVariable* a, ShaderVariable* b)
	{
		bool isSame = true;

		isSame = isSame && (a->offset == b->offset);
		isSame = isSame && (a->size == b->size);
		isSame = isSame && (a->nameSize == b->nameSize);

		// early exit before string cmp
		if (isSame == false)
			return isSame;

		isSame = (strcmp(a->name, b->name) == 0);
		return isSame;
	}

	void destroy_shader_reflection(ShaderReflection* pReflection)
	{
		if (pReflection == nullptr)
			return;

		sg_free(pReflection->pNamePool);
		sg_free(pReflection->pVertexInputs);
		sg_free(pReflection->pShaderResources);
		sg_free(pReflection->pVariables);
	}

	void create_pipeline_reflection(ShaderReflection* pReflection, uint32_t stageCount, PipelineReflection* pOutReflection)
	{
		// parameter checks
		if (pReflection == nullptr)
		{
			SG_LOG_ERROR("Parameter 'pReflection' is nullptr.");
			return;
		}
		if (stageCount == 0)
		{
			SG_LOG_ERROR("Parameter 'stageCount' is 0.");
			return;
		}
		if (pOutReflection == nullptr)
		{
			SG_LOG_ERROR("Parameter 'pOutShaderReflection' is nullptr.");
			return;
		}

		// sanity check to make sure we don't have repeated stages.
		ShaderStage combinedShaderStages = (ShaderStage)0;
		for (uint32_t i = 0; i < stageCount; ++i)
		{
			if ((combinedShaderStages & pReflection[i].shaderStage) != 0)
			{
				SG_LOG_ERROR("Duplicate shader stage was detected in shader reflection array.");
				return;
			}
			combinedShaderStages = (ShaderStage)(combinedShaderStages | pReflection[i].shaderStage);
		}

		// combine all shaders
		// this will have a large amount of looping

		// count number of resources
		uint32_t        vertexStageIndex = ~0u;
		uint32_t        hullStageIndex = ~0u;
		uint32_t        domainStageIndex = ~0u;
		uint32_t        geometryStageIndex = ~0u;
		uint32_t        pixelStageIndex = ~0u;
		ShaderResource* pResources = nullptr;
		uint32_t        resourceCount = 0;
		ShaderVariable* pVariables = nullptr;
		uint32_t        variableCount = 0;

		// should we be using dynamic arrays for these? Perhaps we can add std::vector
		// like functionality?
		ShaderResource* uniqueResources[512];
		ShaderStage shaderUsage[512];
		ShaderVariable* uniqueVariable[512];
		ShaderResource* uniqueVariableParent[512];
		for (uint32_t i = 0; i < stageCount; ++i)
		{
			ShaderReflection* pSrcRef = pReflection + i;
			pOutReflection->stageReflections[i] = *pSrcRef;

			if (pSrcRef->shaderStage == SG_SHADER_STAGE_VERT)
			{
				vertexStageIndex = i;
			}
#if !defined(SG_GRAPHIC_API_METAL)
			else if (pSrcRef->shaderStage == SG_SHADER_STAGE_HULL)
			{
				hullStageIndex = i;
			}
			else if (pSrcRef->shaderStage == SG_SHADER_STAGE_DOMN)
			{
				domainStageIndex = i;
			}
			else if (pSrcRef->shaderStage == SG_SHADER_STAGE_GEOM)
			{
				geometryStageIndex = i;
			}
#endif
			else if (pSrcRef->shaderStage == SG_SHADER_STAGE_FRAG)
			{
				pixelStageIndex = i;
			}

			// loop through all shader resources
			for (uint32_t j = 0; j < pSrcRef->shaderResourceCount; ++j)
			{
				bool unique = true;

				// go through all already added shader resources to see if this shader
				// resource was already added from a different shader stage. If we find a
				// duplicate shader resource, we add the shader stage to the shader stage
				// mask of that resource instead.
				for (uint32_t k = 0; k < resourceCount; ++k)
				{
					unique = !shader_resource_compare(&pSrcRef->pShaderResources[j], uniqueResources[k]);
					if (unique == false)
					{
						// update shader usage
						// NOT SURE
						//shaderUsage[k] = (ShaderStage)(shaderUsage[k] & pSrcRef->pShaderResources[j].used_stages);
						shaderUsage[k] |= pSrcRef->pShaderResources[j].usedStages;
						break;
					}
				}

				// if it's unique, we add it to the list of shader resources
				if (unique == true)
				{
					shaderUsage[resourceCount] = pSrcRef->pShaderResources[j].usedStages;
					uniqueResources[resourceCount] = &pSrcRef->pShaderResources[j];
					resourceCount++;
				}
			}

			// loop through all shader variables (constant/uniform buffer members)
			for (uint32_t j = 0; j < pSrcRef->variableCount; ++j)
			{
				bool unique = true;
				// go through all already added shader variables to see if this shader
				// variable was already added from a different shader stage. If we find a
				// duplicate shader variables, we don't add it.
				for (uint32_t k = 0; k < variableCount; ++k)
				{
					unique = !shader_variable_compare(&pSrcRef->pVariables[j], uniqueVariable[k]);
					if (unique == false)
						break;
				}

				// if it's unique we add it to the list of shader variables
				if (unique)
				{
					uniqueVariableParent[variableCount] = &pSrcRef->pShaderResources[pSrcRef->pVariables[j].parentIndex];
					uniqueVariable[variableCount] = &pSrcRef->pVariables[j];
					variableCount++;
				}
			}
		}

		// copy over the shader resources in a dynamic array of the correct size
		if (resourceCount)
		{
			pResources = (ShaderResource*)sg_calloc(resourceCount, sizeof(ShaderResource));

			for (uint32_t i = 0; i < resourceCount; ++i)
			{
				pResources[i] = *uniqueResources[i];
				pResources[i].usedStages = shaderUsage[i];
			}
		}

		// copy over the shader variables in a dynamic array of the correct size
		if (variableCount)
		{
			pVariables = (ShaderVariable*)sg_malloc(sizeof(ShaderVariable) * variableCount);

			for (uint32_t i = 0; i < variableCount; ++i)
			{
				pVariables[i] = *uniqueVariable[i];
				ShaderResource* parentResource = uniqueVariableParent[i];
				// look for parent
				for (uint32_t j = 0; j < resourceCount; ++j)
				{
					if (shader_resource_compare(&pResources[j], parentResource))
					{
						pVariables[i].parentIndex = j;
						break;
					}
				}
			}
		}

		// all refection structs should be built now
		pOutReflection->shaderStages = combinedShaderStages;

		pOutReflection->stageReflectionCount = stageCount;

		pOutReflection->vertexStageIndex = vertexStageIndex;
		pOutReflection->hullStageIndex = hullStageIndex;
		pOutReflection->domainStageIndex = domainStageIndex;
		pOutReflection->geometryStageIndex = geometryStageIndex;
		pOutReflection->pixelStageIndex = pixelStageIndex;

		pOutReflection->pShaderResources = pResources;
		pOutReflection->shaderResourceCount = resourceCount;

		pOutReflection->pVariables = pVariables;
		pOutReflection->variableCount = variableCount;
	}

	void destroy_pipeline_reflection(PipelineReflection* pReflection)
	{
		if (pReflection == nullptr)
			return;

		for (uint32_t i = 0; i < pReflection->stageReflectionCount; ++i)
			destroy_shader_reflection(&pReflection->stageReflections[i]);

		sg_free(pReflection->pShaderResources);
		sg_free(pReflection->pVariables);
	}

}