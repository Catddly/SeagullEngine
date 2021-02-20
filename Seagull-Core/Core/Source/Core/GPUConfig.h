#pragma once

#include "Interface/ILog.h"
#include "Interface/IFileSystem.h"

#include "../../Renderer/IRenderer/Include/IRenderer.h"

namespace SG
{

	inline GPUPresetLevel string_to_preset_level(const char* presetLevel)
	{
		if (!stricmp(presetLevel, "office"))
			return SG_GPU_PRESET_OFFICE;
		if (!stricmp(presetLevel, "low"))
			return SG_GPU_PRESET_LOW;
		if (!stricmp(presetLevel, "medium"))
			return SG_GPU_PRESET_MEDIUM;
		if (!stricmp(presetLevel, "high"))
			return SG_GPU_PRESET_HIGH;
		if (!stricmp(presetLevel, "ultra"))
			return SG_GPU_PRESET_ULTRA;

		return SG_GPU_PRESET_NONE;
	}

	inline const char* preset_Level_to_string(GPUPresetLevel preset)
	{
		switch (preset)
		{
			case SG_GPU_PRESET_NONE: return "";
			case SG_GPU_PRESET_OFFICE: return "office";
			case SG_GPU_PRESET_LOW: return "low";
			case SG_GPU_PRESET_MEDIUM: return "medium";
			case SG_GPU_PRESET_HIGH: return "high";
			case SG_GPU_PRESET_ULTRA: return "ultra";
			default: return NULL;
		}
	}

	//bool parse_config_line(
	//	const char* pLine, const char* pInVendorId, const char* pInModelId,
	//	char pOutVendorId[SG_MAX_GPU_VENDOR_STRING_LENGTH], char pOutModelId[SG_MAX_GPU_VENDOR_STRING_LENGTH],
	//	char pOutRevisionId[SG_MAX_GPU_VENDOR_STRING_LENGTH], char pOutModelName[SG_MAX_GPU_VENDOR_STRING_LENGTH],
	//	GPUPresetLevel* pOutPresetLevel)
	//{
	//	//     VendorId;         ModelId;        Preset;             Name;               RevisionId (optional); Codename (can be null)
	//	// ([0xA-Fa-f0-9]+); ([0xA-Fa-f0-9]+); ([A-Za-z]); ([A-Za-z0-9 /\\(\\);]+)[; ]*([0xA-Fa-f0-9]*)
	//	char buffer[128] = {};
	//	sprintf(
	//		buffer,
	//		"%s; %s; ([A-Za-z]+); ([A-Za-z0-9 /\\(\\);]+)[; ]*([0xA-Fa-f0-9]*)",
	//		// If input is unspecified it means we need to fill it
	//		pInVendorId ? pInVendorId : "([0xA-Fa-f0-9]+)",
	//		pInModelId ? pInModelId : "([0xA-Fa-f0-9]+)");

	//	const uint32_t presetIndex = pInVendorId ? 1 : 3;
	//	const uint32_t gpuNameIndex = pInVendorId ? 2 : 4;
	//	const uint32_t revisionIndex = pInVendorId ? 3 : 5;

	//	std::regex expr(buffer, std::regex::optimize);
	//	std::cmatch match;
	//	if (std::regex_match(pLine, match, expr))
	//	{
	//		if (!pInVendorId)
	//		{
	//			strncpy(pOutVendorId, match[1].first, match[1].second - match[1].first);
	//			strncpy(pOutModelId, match[2].first, match[2].second - match[2].first);
	//		}

	//		char presetLevel[SG_MAX_GPU_VENDOR_STRING_LENGTH] = {};
	//		strncpy(presetLevel, match[presetIndex].first, match[presetIndex].second - match[presetIndex].first);
	//		strncpy(pOutModelName, match[gpuNameIndex].first, match[gpuNameIndex].second - match[gpuNameIndex].first);
	//		strncpy(pOutRevisionId, match[revisionIndex].first, match[revisionIndex].second - match[revisionIndex].first);

	//		*pOutPresetLevel = string_to_preset_level(presetLevel);
	//		return true;
	//	}

	//	*pOutPresetLevel = SG_GPU_PRESET_LOW;
	//	return false;
	//}

	//GPUPresetLevel get_single_preset_level(const char* line, const char* inVendorId, const char* inModelId, const char* inRevId)
	//{
	//	char vendorId[SG_MAX_GPU_VENDOR_STRING_LENGTH] = {};
	//	char deviceId[SG_MAX_GPU_VENDOR_STRING_LENGTH] = {};
	//	GPUPresetLevel presetLevel = {};
	//	char gpuName[SG_MAX_GPU_VENDOR_STRING_LENGTH] = {};
	//	char revisionId[SG_MAX_GPU_VENDOR_STRING_LENGTH] = {};

	//	//check if current vendor line is one of the selected gpu's
	//	if (!parse_config_line(line, inVendorId, inModelId, vendorId, deviceId, revisionId, gpuName, &presetLevel))
	//		return SG_GPU_PRESET_NONE;

	//	//if we have a revision Id then we want to match it as well
	//	if (stricmp(inRevId, "0x00") && strlen(revisionId) && stricmp(revisionId, "0x00") && stricmp(inRevId, revisionId))
	//		return SG_GPU_PRESET_NONE;

	//	return presetLevel;
	//}

	////Reads the gpu config and sets the preset level of all available gpu's
	//GPUPresetLevel get_GPU_preset_level(const eastl::string& vendorId, const eastl::string& modelId, const eastl::string& revId)
	//{
	//	SG_LOG_INFO("No gpu.cfg support. Preset set to Low");
	//	GPUPresetLevel foundLevel = SG_GPU_PRESET_LOW;
	//	return foundLevel;
	//}

	////Reads the gpu config and sets the preset level of all available gpu's
	//GPUPresetLevel get_GPU_preset_level(const char* vendorId, const char* modelId, const char* revId)
	//{
	//	FileStream fh = {};
	//	if (!SG::sgfs_open_stream_from_path(SG_RD_GPU_CONFIG, "gpu.cfg", SG_FM_READ, &fh))
	//	{
	//		SG_LOG_WARING("gpu.cfg could not be found, setting preset to Low as a default.");
	//		return SG_GPU_PRESET_LOW;
	//	}

	//	GPUPresetLevel foundLevel = SG_GPU_PRESET_LOW;

	//	char gpuCfgString[1024] = {};
	//	while (!SG::sgfs_is_stream_at_end(&fh))
	//	{
	//		SG::sgfs_read_from_stream(&fh, gpuCfgString);
	//		GPUPresetLevel level = get_single_preset_level(gpuCfgString, vendorId, modelId, revId);
	//		// Do something with the tok
	//		if (level != SG_GPU_PRESET_NONE)
	//		{
	//			foundLevel = level;
	//			break;
	//		}
	//	}

	//	sgfs_close_stream(&fh);
	//	return foundLevel;
	//}

}

