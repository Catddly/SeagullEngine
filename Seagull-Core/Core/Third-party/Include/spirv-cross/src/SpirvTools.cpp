#include "../include/SpirvTools.h"

#define SPIRV_CROSS_EXCEPTIONS_TO_ASSERTIONS

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4996)    // warning C4996: 'std::move_backward::_Unchecked_iterators::_Deprecate'
#endif

#include "../src/spirv_cross.hpp"

// helper functions
void reflect_bound_resources(
	spirv_cross::Compiler* pCompiler,
	const spirv_cross::SmallVector<spirv_cross::Resource>& allResources,
	const std::unordered_set<spirv_cross::VariableID>& usedResouces,
	SpirvResource* resources,
	uint32_t* currentResource,
	SpirvResourceType sprivType)
{
	for (size_t i = 0; i < allResources.size(); ++i)
	{
		spirv_cross::Resource const& input = allResources[i];
		SpirvResource& resource = resources[(*currentResource)++];

		resource.spirvCode.id = input.id;
		resource.spirvCode.typeId = input.type_id;
		resource.spirvCode.baseTypeId = input.base_type_id;

		resource.type = sprivType;

		resource.isUsed = (usedResouces.count(resource.spirvCode.id) != 0);

		resource.set = pCompiler->get_decoration(resource.spirvCode.id, spv::DecorationDescriptorSet);
		resource.binding = pCompiler->get_decoration(resource.spirvCode.id, spv::DecorationBinding);

		spirv_cross::SPIRType type = pCompiler->get_type(resource.spirvCode.typeId);

		// special case for textureBuffer / imageBuffer
		// textureBuffer is considered as separate image  with dimension buffer in SpirV but they require a buffer descriptor of type uniform texel buffer
		// imageBuffer is considered as storage image with dimension buffer in SpirV but they require a buffer descriptor of type storage texel buffer
		if (type.image.dim == spv::Dim::DimBuffer)
		{
			if (sprivType == SPIRV_TYPE_IMAGES)
				resource.type = SPIRV_TYPE_UNIFORM_TEXEL_BUFFERS;
			else if (sprivType == SPIRV_TYPE_STORAGE_IMAGES)
				resource.type = SPIRV_TYPE_STORAGE_TEXEL_BUFFERS;
		}

		// set the texture dimensions
		switch (type.image.dim)
		{
		case spv::DimBuffer:
			resource.dim = SPIRV_DIM_BUFFER;
			break;
		case spv::Dim1D:
			resource.dim = type.image.arrayed ? SPIRV_DIM_TEXTURE1DARRAY : SPIRV_DIM_TEXTURE1D;
			break;
		case spv::Dim2D:
			if (type.image.ms)
				resource.dim = type.image.arrayed ? SPIRV_DIM_TEXTURE2DMSARRAY : SPIRV_DIM_TEXTURE2DMS;
			else
				resource.dim = type.image.arrayed ? SPIRV_DIM_TEXTURE2DARRAY : SPIRV_DIM_TEXTURE2D;
			break;
		case spv::Dim3D:
			resource.dim = SPIRV_DIM_TEXTURE3D;
			break;
		case spv::DimCube:
			resource.dim = type.image.arrayed ? SPIRV_DIM_TEXTURECUBEARRAY : SPIRV_DIM_TEXTURECUBE;
			break;
		default:
			resource.dim = SPIRV_DIM_UNDEFINED;
			break;
		}

		//if(sprivType != SPIRV_TYPE_UNIFORM_BUFFERS)
		{
			if (type.array.size())
				resource.size = type.array[0];
			else
				resource.size = 1;
		}
		//else
		//{
		//   resource.size = (uint32_t)pCompiler->get_declared_struct_size(type);
		//}

		// use the instance name if there is one
		std::string name = pCompiler->get_name(resource.spirvCode.id);
		if (name.empty())
			name = input.name;

		resource.nameSize = (uint32_t)name.size();
		resource.name = new char[resource.nameSize + 1];
		// name is a const char * but we just allocated it so it is fine to modify it now
		memcpy((char*)resource.name, name.data(), resource.nameSize);
		((char*)resource.name)[resource.nameSize] = 0;
	}
}


void create_cross_complier(uint32_t const* spirvBinary, uint32_t binarySize, CrossCompiler* outCompiler)
{
	if (outCompiler == nullptr)
	{
		return; // error code here
	}
	// build the compiler
	spirv_cross::Compiler* compiler = new spirv_cross::Compiler(spirvBinary, binarySize);

	outCompiler->pCompiler = compiler;

	outCompiler->pShaderResouces = nullptr;
	outCompiler->ShaderResourceCount = 0;

	outCompiler->pUniformVariables = nullptr;
	outCompiler->uniformVariablesCount = 0;
}

void destroy_cross_complier(CrossCompiler* pCompiler)
{
	if (pCompiler == nullptr)
	{
		return; // error code here
	}

	spirv_cross::Compiler* compiler = (spirv_cross::Compiler*)pCompiler->pCompiler;

	delete compiler;

	for (uint32_t i = 0; i < pCompiler->ShaderResourceCount; ++i)
	{
		delete[] pCompiler->pShaderResouces[i].name;
	}

	delete[] pCompiler->pShaderResouces;

	for (uint32_t i = 0; i < pCompiler->uniformVariablesCount; ++i)
	{
		delete[] pCompiler->pUniformVariables[i].name;
	}

	delete[] pCompiler->pUniformVariables;
	delete[] pCompiler->pEntryPoint;

	pCompiler->pCompiler = nullptr;
	pCompiler->pShaderResouces = nullptr;
	pCompiler->ShaderResourceCount = 0;
	pCompiler->pUniformVariables = nullptr;
	pCompiler->uniformVariablesCount = 0;
}

void reflect_entry_point(CrossCompiler* pCompiler)
{
	if (pCompiler == nullptr)
	{
		return; // error code here
	}

	spirv_cross::Compiler* compiler = (spirv_cross::Compiler*)pCompiler->pCompiler;
	std::string entryPoint = compiler->get_entry_points_and_stages()[0].name;

	pCompiler->entryPointSize = (uint32_t)entryPoint.size();

	pCompiler->pEntryPoint = new char[pCompiler->entryPointSize + 1];
	memcpy(pCompiler->pEntryPoint, entryPoint.c_str(), pCompiler->entryPointSize * sizeof(char));
	pCompiler->pEntryPoint[pCompiler->entryPointSize] = 0;
}

void reflect_shader_resources(CrossCompiler* pCompiler)
{
	if (pCompiler == nullptr)
	{
		return; // error code here
	}
	spirv_cross::Compiler* compiler = (spirv_cross::Compiler*)pCompiler->pCompiler;

	// 1. get all shader resources
	spirv_cross::ShaderResources allResources;
	std::unordered_set<spirv_cross::VariableID> usedResouces;

	allResources = compiler->get_shader_resources();
	usedResouces = compiler->get_active_interface_variables();

	// 2. Count number of resources and allocate array
	uint32_t resourceCount = 0;
	// resources we want to reflect
	resourceCount += (uint32_t)allResources.stage_inputs.size();			// inputs
	resourceCount += (uint32_t)allResources.stage_outputs.size();		    // outputs
	resourceCount += (uint32_t)allResources.uniform_buffers.size();		    // const buffers
	resourceCount += (uint32_t)allResources.storage_buffers.size();		    // buffers
	resourceCount += (uint32_t)allResources.separate_images.size();		    // textures
	resourceCount += (uint32_t)allResources.storage_images.size();		    // uav textures
	resourceCount += (uint32_t)allResources.separate_samplers.size();	    // samplers
	resourceCount += (uint32_t)allResources.sampled_images.size();          // combined samplers
	resourceCount += (uint32_t)allResources.push_constant_buffers.size();   // push constants
	resourceCount += (uint32_t)allResources.subpass_inputs.size();          // input attachments
	resourceCount += (uint32_t)allResources.acceleration_structures.size(); // raytracing structures

	// these we don't care about right
	// subpass_inputs - we are not going to use this
	// atomic_counters - not usable in Vulkan

	//if (!allResources.subpass_inputs.size())
	//	ASSERT(false);
		//SG_LOG_WARNING("Found subpass_inputs in (%s) shader", pCompiler->pShaderResouces->name);	
	//if (!allResources.atomic_counters.size())
	//	ASSERT(false);
		//SG_LOG_WARNING("Found atomic_counters in (%s) shader", pCompiler->pShaderResouces->name);

	// allocate array for resources
	SpirvResource* resources = new SpirvResource[resourceCount];

	uint32_t currentResource = 0;

	// 3. start by reflecting the shader inputs
	for (size_t i = 0; i < allResources.stage_inputs.size(); ++i)
	{
		spirv_cross::Resource const& input = allResources.stage_inputs[i];
		SpirvResource& resource = resources[currentResource++];

		resource.spirvCode.id = input.id;
		resource.spirvCode.typeId = input.type_id;
		resource.spirvCode.baseTypeId = input.base_type_id;

		resource.type = SPIRV_TYPE_STAGE_INPUTS;

		resource.isUsed = (usedResouces.count(resource.spirvCode.id) != 0);

		resource.set = uint32_t(-1); // stage inputs don't have sets
		resource.binding = compiler->get_decoration(resource.spirvCode.id, spv::DecorationLocation); // location is the binding point for inputs

		spirv_cross::SPIRType type = compiler->get_type(resource.spirvCode.typeId);
		// bit width * vecsize = size
		resource.size = (type.width / 8) * type.vecsize;

		resource.nameSize = (uint32_t)input.name.size();
		resource.name = new char[resource.nameSize + 1];
		// name is a const char * but we just allocated it so it is fine to modify it now
		memcpy((char*)resource.name, input.name.data(), resource.nameSize);
		((char*)resource.name)[resource.nameSize] = 0;
	}

	// 4. reflect output
	for (size_t i = 0; i < allResources.stage_outputs.size(); ++i)
	{
		spirv_cross::Resource const& input = allResources.stage_outputs[i];
		SpirvResource& resource = resources[currentResource++];

		resource.spirvCode.id = input.id;
		resource.spirvCode.typeId = input.type_id;
		resource.spirvCode.baseTypeId = input.base_type_id;

		resource.type = SPIRV_TYPE_STAGE_OUTPUTS;

		resource.isUsed = (usedResouces.count(resource.spirvCode.id) != 0);

		resource.set = uint32_t(-1); // stage outputs don't have sets
		resource.binding = compiler->get_decoration(resource.spirvCode.id, spv::DecorationLocation); // location is the binding point for inputs

		spirv_cross::SPIRType type = compiler->get_type(resource.spirvCode.typeId);
		// bit width * vecsize = size
		resource.size = (type.width / 8) * type.vecsize;

		resource.nameSize = (uint32_t)input.name.size();
		resource.name = new char[resource.nameSize + 1];
		// name is a const char * but we just allocated it so it is fine to modify it now
		memcpy((char*)resource.name, input.name.data(), resource.nameSize);
		((char*)resource.name)[resource.nameSize] = 0;
	}

	// 5. reflect the 'normal' resources
	reflect_bound_resources(compiler, allResources.uniform_buffers, usedResouces, resources, &currentResource, SPIRV_TYPE_UNIFORM_BUFFERS);
	reflect_bound_resources(compiler, allResources.storage_buffers, usedResouces, resources, &currentResource, SPIRV_TYPE_STORAGE_BUFFERS);
	reflect_bound_resources(compiler, allResources.storage_images, usedResouces, resources, &currentResource, SPIRV_TYPE_STORAGE_IMAGES);
	reflect_bound_resources(compiler, allResources.separate_images, usedResouces, resources, &currentResource, SPIRV_TYPE_IMAGES);
	reflect_bound_resources(compiler, allResources.separate_samplers, usedResouces, resources, &currentResource, SPIRV_TYPE_SAMPLERS);
	reflect_bound_resources(compiler, allResources.sampled_images, usedResouces, resources, &currentResource, SPIRV_TYPE_COMBINED_SAMPLERS);
	//reflect_bound_resources(compiler, allResources.subpass_inputs, usedResouces, resources, &currentResource, SPIRV_TYPE_SUBPASS_INPUTS);
	reflect_bound_resources(compiler, allResources.subpass_inputs, usedResouces, resources, &currentResource, SPIRV_TYPE_SUBPASS_INPUTS);
	reflect_bound_resources(compiler, allResources.acceleration_structures, usedResouces, resources, &currentResource, SPIRV_TYPE_ACCELERATION_STRUCTURES);

	// 6. reflect push buffers
	for (size_t i = 0; i < allResources.push_constant_buffers.size(); ++i)
	{
		spirv_cross::Resource const& input = allResources.push_constant_buffers[i];
		SpirvResource& resource = resources[currentResource++];

		resource.spirvCode.id = input.id;
		resource.spirvCode.typeId = input.type_id;
		resource.spirvCode.baseTypeId = input.base_type_id;

		resource.type = SPIRV_TYPE_PUSH_CONSTANT;

		resource.isUsed = (usedResouces.count(resource.spirvCode.id) != 0);

		resource.set = uint32_t(-1); // push consts don't have sets
		resource.binding = uint32_t(-1); // push consts don't have bindings

		spirv_cross::SPIRType type = compiler->get_type(resource.spirvCode.typeId);
		resource.size = (uint32_t)compiler->get_declared_struct_size(type);

		resource.nameSize = (uint32_t)input.name.size();
		resource.name = new char[resource.nameSize + 1];
		resource.dim = SPIRV_DIM_UNDEFINED;
		// name is a const char * but we just allocated it so it is fine to modify it now
		memcpy((char*)resource.name, input.name.data(), resource.nameSize);
		((char*)resource.name)[resource.nameSize] = 0;
	}

	pCompiler->pShaderResouces = resources;
	pCompiler->ShaderResourceCount = resourceCount;
}

void reflect_shader_variables(CrossCompiler* pCompiler)
{
	if (pCompiler == nullptr)
	{
		return; // error code here
	}
	if (pCompiler->ShaderResourceCount == 0)
	{
		return; // error code here
	}
	spirv_cross::Compiler* compiler = (spirv_cross::Compiler*)pCompiler->pCompiler;

	// 1. count number of variables we have
	uint32_t variableCount = 0;

	for (uint32_t i = 0; i < pCompiler->ShaderResourceCount; ++i)
	{
		SpirvResource& resource = pCompiler->pShaderResouces[i];

		if (resource.type == SPIRV_TYPE_UNIFORM_BUFFERS || resource.type == SPIRV_TYPE_PUSH_CONSTANT)
		{
			spirv_cross::SPIRType type = compiler->get_type(resource.spirvCode.typeId);
			variableCount += (uint32_t)type.member_types.size();
		}
	}

	// 2. allocate memory
	SpirvVariable* variables = new SpirvVariable[variableCount];
	uint32_t current_variable = 0;

	// 3. reflect
	for (uint32_t i = 0; i < pCompiler->ShaderResourceCount; ++i)
	{
		SpirvResource& resource = pCompiler->pShaderResouces[i];

		if (resource.type == SPIRV_TYPE_UNIFORM_BUFFERS || resource.type == SPIRV_TYPE_PUSH_CONSTANT)
		{
			uint32_t start_of_block = current_variable;

			spirv_cross::SPIRType type = compiler->get_type(resource.spirvCode.typeId);
			for (uint32_t j = 0; j < (uint32_t)type.member_types.size(); ++j)
			{
				SpirvVariable& variable = variables[current_variable++];

				variable.spirvTypeId = type.member_types[j];

				variable.parentSpirvCode = resource.spirvCode;
				variable.parentIndex = i;

				variable.isUsed = false;

				variable.size = (uint32_t)compiler->get_declared_struct_member_size(type, j);
				variable.offset = compiler->get_member_decoration(resource.spirvCode.baseTypeId, j, spv::DecorationOffset);

				std::string member_name = compiler->get_member_name(resource.spirvCode.baseTypeId, j);
				variable.nameSize = (uint32_t)member_name.size();
				variable.name = new char[variable.nameSize + 1];
				// name is a const char * but we just allocated it so it is fine to modify it now
				memcpy((char*)variable.name, member_name.data(), variable.nameSize);
				((char*)variable.name)[variable.nameSize] = 0;
			}

			spirv_cross::SmallVector<spirv_cross::BufferRange> range = compiler->get_active_buffer_ranges(resource.spirvCode.id);

			for (uint32_t j = 0; j < (uint32_t)range.size(); ++j)
			{
				variables[start_of_block + range[j].index].isUsed = true;
			}
		}
	}

	pCompiler->pUniformVariables = variables;
	pCompiler->uniformVariablesCount = variableCount;
}

void reflect_compute_shader_work_groups_size(CrossCompiler* pCompiler, uint32_t* pSizeX, uint32_t* pSizeY, uint32_t* pSizeZ)
{
	spirv_cross::Compiler* compiler = (spirv_cross::Compiler*)pCompiler->pCompiler;
	spirv_cross::SPIREntryPoint* pEntryPoint = &compiler->get_entry_point(pCompiler->pEntryPoint, compiler->get_execution_model());

	*pSizeX = pEntryPoint->workgroup_size.x;
	*pSizeY = pEntryPoint->workgroup_size.y;
	*pSizeZ = pEntryPoint->workgroup_size.z;
}

void reflect_hull_shader_control_point(CrossCompiler* pCompiler, uint32_t* pSizeX)
{
	spirv_cross::Compiler* compiler = (spirv_cross::Compiler*)pCompiler->pCompiler;
	spirv_cross::SPIREntryPoint* pEntryPoint = &compiler->get_entry_point(pCompiler->pEntryPoint, compiler->get_execution_model());

	*pSizeX = pEntryPoint->output_vertices;
}

#if defined(_MSC_VER)
#pragma warning(pop)
#endif
