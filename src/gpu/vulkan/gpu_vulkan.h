/* date = May 14th 2025 1:39 pm */

#ifndef GPU_VULKAN_H
#define GPU_VULKAN_H

#include "third_party/vulkan/vulkan/vulkan.h"

struct GPU_Buffer
{
  VkBuffer buffer;
  VkDeviceMemory memory;
  U64 size;
  void* mapped_ptr;
};

struct GPU_Kernel
{
  String8 name;
  
  VkShaderModule shader;
  VkPipeline pipeline;
  VkPipelineLayout pipeline_layout;
  
  VkDescriptorSetLayout descriptor_set_layout;
  VkDescriptorSet descriptor_set;
  
  GPU_Buffer* bound_buffers[16];
  U32 bound_buffer_count;
};

struct GPU_State
{
  Arena* arena;
  
  VkInstance instance;
  VkPhysicalDevice physical_device;
  VkDevice device;
  
  VkQueue compute_queue;
  U32 compute_queue_family_index;
  
  VkCommandPool command_pool;
  VkCommandBuffer command_buffer;
  
  VkDescriptorPool descriptor_pool;
};

global GPU_State* g_vulkan_state = 0;

#endif //GPU_VULKAN_H
