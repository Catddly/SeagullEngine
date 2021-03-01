#pragma once

#include <include/glm.hpp>

#include <include/detail/qualifier.hpp>

namespace SG
{

	typedef glm::vec2  Vec2;
	typedef glm::vec3  Vec3;
	typedef glm::vec4  Vec4;

	typedef glm::ivec2 IVec2;
	typedef glm::ivec3 IVec3;
	typedef glm::ivec4 IVec4;

	typedef glm::fvec2 FVec2;
	typedef glm::fvec3 FVec3;
	typedef glm::fvec4 FVec4;

	typedef glm::mat2  Matrix2;
	typedef glm::mat3  Matrix3;
	typedef glm::mat4  Matrix4;

}

template <typename T>
static inline size_t sg_mem_hash(const T* mem, size_t size, size_t prev = 2166136261U)
{
	uint32_t result = (uint32_t)prev; // Intentionally uint32_t instead of size_t, so the behavior is the same
									  // regardless of size.
	while (size--)
		result = (result * 16777619) ^ *mem++; // FNV hash
	return (size_t)result;
}

namespace eastl
{

	template<typename T, glm::qualifier Q>
	struct hash<glm::vec<2, T, Q> >
	{
		size_t operator()(glm::vec<2, T, Q> const& v) const
		{
			size_t seed = 2166136261U;
			seed = sg_mem_hash((uint32_t*)&v.x, sizeof(uint32_t));
			seed = sg_mem_hash((uint32_t*)&v.y, sizeof(uint32_t));
			return seed;
		}
	};

	template<typename T, glm::qualifier Q>
	struct hash<glm::vec<3, T, Q> >
	{
		size_t operator()(glm::vec<3, T, Q> const& v) const
		{
			size_t seed = 2166136261U;
			seed = sg_mem_hash((uint32_t*)&v.x, sizeof(uint32_t), seed);
			seed = sg_mem_hash((uint32_t*)&v.y, sizeof(uint32_t), seed);
			seed = sg_mem_hash((uint32_t*)&v.z, sizeof(uint32_t), seed);
			return seed;
		}
	};
}
