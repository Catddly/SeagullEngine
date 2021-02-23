#pragma once

#include <stdint.h>

namespace SG
{

	static const uint32_t MAX_SHADER_STAGE_COUNT = 5;

	typedef enum TextureDimension
	{
		SG_TEXTURE_DIM_1D = 0,
		SG_TEXTURE_DIM_2D,
		SG_TEXTURE_DIM_2DMS,
		SG_TEXTURE_DIM_3D,
		SG_TEXTURE_DIM_CUBE,
		SG_TEXTURE_DIM_1D_ARRAY,
		SG_TEXTURE_DIM_2D_ARRAY,
		SG_TEXTURE_DIM_2DMS_ARRAY,
		SG_TEXTURE_DIM_CUBE_ARRAY,
		SG_TEXTURE_DIM_COUNT,
		SG_TEXTURE_DIM_UNDEFINED,
	} TextureDimension;

	struct VertexInput
	{
		// The size of the attribute
		uint32_t    size;
		// resource name
		const char* name;
		// name size
		uint32_t    nameSize;
	};

	struct ShaderResource
	{
		// resource Type
		DescriptorType type;
		// The resource set for binding frequency
		uint32_t set;
		// The resource binding location
		uint32_t reg;
		// The size of the resource. This will be the DescriptorInfo array size for textures
		uint32_t size;
		// what stages use this resource
		ShaderStage usedStages;
		// resource name
		const char* name;
		// name size
		uint32_t nameSize;
		// 1D / 2D / Array / MSAA / ...
		TextureDimension dim;
	};

	struct ShaderVariable
	{
		// parents resource index
		uint32_t parentIndex;
		// The offset of the Variable.
		uint32_t offset;
		// The size of the Variable.
		uint32_t size;
		// Variable name
		const char* name;
		// name size
		uint32_t nameSize;
#if defined(SG_GRAPHIC_API_GLES)
		GLenum type; // Needed to use the right glUniform(i) function to upload the data
#endif
	};

	struct ShaderReflection
	{
		ShaderStage shaderStage;
		// single large allocation for names to reduce number of allocations
		char*	    pNamePool;
		uint32_t    namePoolSize;

		VertexInput* pVertexInputs;
		uint32_t     vertexInputsCount;

		ShaderResource* pShaderResources;
		uint32_t        shaderResourceCount;

		ShaderVariable* pVariables;
		uint32_t        variableCount;

		// Thread group size for compute shader
		uint32_t numThreadsPerGroup[3];
		//number of tessellation control point
		uint32_t numControlPoint;
#if defined(SG_GRAPHIC_API_VULKAN)
		char* pEntryPoint;
#endif
	};

	struct PipelineReflection
	{
		ShaderStage shaderStages;
		// the individual stages reflection data.
		ShaderReflection stageReflections[MAX_SHADER_STAGE_COUNT];
		uint32_t         stageReflectionCount;

		uint32_t vertexStageIndex;
		uint32_t hullStageIndex;
		uint32_t domainStageIndex;
		uint32_t geometryStageIndex;
		uint32_t pixelStageIndex;

		ShaderResource* pShaderResources;
		uint32_t        shaderResourceCount;

		ShaderVariable* pVariables;
		uint32_t        variableCount;
	};

	void destroy_shader_reflection(ShaderReflection* pReflection);

	void create_pipeline_reflection(ShaderReflection* pReflection, uint32_t stageCount, PipelineReflection* pOutReflection);
	void destroy_pipeline_reflection(PipelineReflection* pReflection);

	//void serialize_reflection(File* pInFile, Reflection* pReflection);
	//void deserialize_reflection(File* pOutFile, Reflection* pReflection);

}