#pragma once

#include "Interface/IOperatingSystem.h"

struct SpirvType
{
	// Resources are identified with their SPIR-V ID.
	// This is the ID of the OpVariable.
	uint32_t id;

	// The type ID of the variable which includes arrays and all type modifications.
	// This type ID is not suitable for parsing OpMemberDecoration of a struct and other decorations in general
	// since these modifications typically happen on the base_type_id.
	uint32_t typeId;

	// The base type of the declared resource.
	// This type is the base type which ignores pointers and arrays of the type_id.
	// This is mostly useful to parse decorations of the underlying type.
	// base_type_id can also be obtained with get_type(get_type(type_id).self).
	uint32_t baseTypeId;
};

enum SpirvResourceType
{
	SPIRV_TYPE_STAGE_INPUTS = 0,
	SPIRV_TYPE_STAGE_OUTPUTS,
	SPIRV_TYPE_UNIFORM_BUFFERS,
	SPIRV_TYPE_STORAGE_BUFFERS,
	SPIRV_TYPE_IMAGES,
	SPIRV_TYPE_STORAGE_IMAGES,
	SPIRV_TYPE_SAMPLERS,
	SPIRV_TYPE_PUSH_CONSTANT,
	SPIRV_TYPE_SUBPASS_INPUTS,
	SPIRV_TYPE_UNIFORM_TEXEL_BUFFERS,
	SPIRV_TYPE_STORAGE_TEXEL_BUFFERS,
	SPIRV_TYPE_ACCELERATION_STRUCTURES,
	SPIRV_TYPE_COMBINED_SAMPLERS,
	SPIRV_TYPE_COUNT
};

enum SpirvResourceDim
{
	SPIRV_DIM_UNDEFINED = 0,
	SPIRV_DIM_BUFFER = 1,
	SPIRV_DIM_TEXTURE1D = 2,
	SPIRV_DIM_TEXTURE1DARRAY = 3,
	SPIRV_DIM_TEXTURE2D = 4,
	SPIRV_DIM_TEXTURE2DARRAY = 5,
	SPIRV_DIM_TEXTURE2DMS = 6,
	SPIRV_DIM_TEXTURE2DMSARRAY = 7,
	SPIRV_DIM_TEXTURE3D = 8,
	SPIRV_DIM_TEXTURECUBE = 9,
	SPIRV_DIM_TEXTURECUBEARRAY = 10,
	SPIRV_DIM_COUNT = 11,
};

struct SpirvResource
{
	// spirv data type
	SpirvType spirvCode;

	// resource type
	SpirvResourceType type;

	// texture dimension. Undefined if not a texture.
	SpirvResourceDim dim;

	// If the resouce was used in the shader
	bool isUsed;

	// The resouce set if it has one
	uint32_t set;

	// The resource binding location
	uint32_t binding;

	// The size of the resouce. This will be the descriptor array size for textures
	uint32_t size;

	// The declared name (OpName) of the resource.
	// For Buffer blocks, the name actually reflects the externally
	// visible Block name.
	const char* name;

	// name size
	uint32_t nameSize;
};

struct SpirvVariable
{
	// Spirv data type
	uint32_t spirvTypeId;

	// parents SPIRV code
	SpirvType parentSpirvCode;

	// parents resource index
	uint32_t parentIndex;

	// If the data was used
	bool isUsed;

	// The offset of the Variable.
	uint32_t offset;

	// The size of the Variable.
	uint32_t size;

	// Variable name
	const char* name;

	// name size
	uint32_t nameSize;
};

struct CrossCompiler
{
	// this points to the internal compiler class
	void* pCompiler;

	// resources
	SpirvResource* pShaderResouces;
	uint32_t ShaderResourceCount;

	// uniforms
	SpirvVariable* pUniformVariables;
	uint32_t uniformVariablesCount;

	char* pEntryPoint;
	uint32_t entryPointSize;
};

void create_cross_complier(const uint32_t* SpirvBinary, uint32_t BinarySize, CrossCompiler* outCompiler);
void destroy_cross_complier(CrossCompiler* compiler);

void reflect_entry_point(CrossCompiler* compiler);

void reflect_shader_resources(CrossCompiler* compiler);
void reflect_shader_variables(CrossCompiler* compiler);

void reflect_compute_shader_work_groups_size(CrossCompiler* compiler, uint32_t* pSizeX, uint32_t* pSizeY, uint32_t* pSizeZ);
void reflect_hull_shader_control_point(CrossCompiler* pCompiler, uint32_t* pSizeX);
