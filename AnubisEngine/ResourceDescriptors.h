#pragma once
#include <glm/glm.hpp>

// scalars - 4 bytes
// float2 - 8 bytes
// float3/float4 - 16 bytes
// nested struct must be aligned by the base alignment of its members (rounded to multiple of 16)
// float4x4 - 16 bytes

// UniformBufferObject - defines: model, view, and project matrices
struct UniformBufferObject
{
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
};

// tut covered combined image samplers
// for sampler reuse, use samplers (VK_DESCRIPTOR_TYPE_SAMPLER) and sampled images (VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE)
