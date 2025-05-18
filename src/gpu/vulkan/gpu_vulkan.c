internal U32
gpu_vulkan_find_memory_type(U32 type_bits, VkMemoryPropertyFlags props)
{
  VkPhysicalDeviceMemoryProperties mem_props;
  vkGetPhysicalDeviceMemoryProperties(g_vulkan_state->physical_device, &mem_props);
  
  for (U32 i = 0; i < mem_props.memoryTypeCount; i++)
  {
    if ((type_bits & (1 << i)) && (mem_props.memoryTypes[i].propertyFlags & props) == props)
    {
      return i;
    }
  }
  
  log_error("failed to find suitable Vulkan memory type.");
  return max_U32;
}

internal void
gpu_init(void)
{
  ProfBeginFunction();
  
  Arena* arena = arena_alloc();
  g_vulkan_state = push_array(arena, GPU_State, 1);
  g_vulkan_state->arena = arena;
  
  VkResult res;
  
  //- tec: instance
  VkApplicationInfo app_info =
  {
    .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
    .pApplicationName = "GPU SQL Engine",
    .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
    .pEngineName = "gdb",
    .engineVersion = VK_MAKE_VERSION(1, 0, 0),
    .apiVersion = VK_API_VERSION_1_3,
  };
  
  VkInstanceCreateInfo inst_info =
  {
    .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
    .pApplicationInfo = &app_info,
  };
  
  res = vkCreateInstance(&inst_info, 0, &g_vulkan_state->instance);
  if (res != VK_SUCCESS)
  {
    log_error("failed to create Vulkan instance");
    return;
  }
  
  //- tec: physical device
  U32 dev_count = 0;
  vkEnumeratePhysicalDevices(g_vulkan_state->instance, &dev_count, 0);
  VkPhysicalDevice *devices = push_array(arena, VkPhysicalDevice, dev_count);
  vkEnumeratePhysicalDevices(g_vulkan_state->instance, &dev_count, devices);
  
  VkPhysicalDevice selected = 0;
  U32 compute_queue_family_index = ~0u;
  for (U32 i = 0; i < dev_count; i++)
  {
    U32 queue_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(devices[i], &queue_count, 0);
    VkQueueFamilyProperties *props = push_array(arena, VkQueueFamilyProperties, queue_count);
    vkGetPhysicalDeviceQueueFamilyProperties(devices[i], &queue_count, props);
    
    for (U32 j = 0; j < queue_count; j++)
    {
      if (props[j].queueFlags & VK_QUEUE_COMPUTE_BIT) 
      {
        selected = devices[i];
        compute_queue_family_index = j;
        break;
      }
    }
    if (selected) break;
  }
  
  if (!selected)
  {
    log_error("no suitable Vulkan GPU found");
    return;
  }
  
  g_vulkan_state->physical_device = selected;
  g_vulkan_state->compute_queue_family_index = compute_queue_family_index;
  
  //- tec: logical device
  F32 priority = 1.0f;
  VkDeviceQueueCreateInfo queue_info = 
  {
    .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
    .queueFamilyIndex = compute_queue_family_index,
    .queueCount = 1,
    .pQueuePriorities = &priority,
  };
  
  VkDeviceCreateInfo dev_info = 
  {
    .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
    .queueCreateInfoCount = 1,
    .pQueueCreateInfos = &queue_info,
  };
  
  res = vkCreateDevice(selected, &dev_info, 0, &g_vulkan_state->device);
  if (res != VK_SUCCESS) 
  {
    log_error("failed to create Vulkan logical device");
    return;
  }
  
  vkGetDeviceQueue(g_vulkan_state->device, compute_queue_family_index, 0, &g_vulkan_state->compute_queue);
  
  //- tec: command pool
  VkCommandPoolCreateInfo pool_info =
  {
    .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
    .queueFamilyIndex = compute_queue_family_index,
    .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
  };
  
  res = vkCreateCommandPool(g_vulkan_state->device, &pool_info, 0, &g_vulkan_state->command_pool);
  if (res != VK_SUCCESS)
  {
    log_error("failed to create Vulkan command pool");
    return;
  }
  
  //- tec: command buffer
  VkCommandBufferAllocateInfo alloc_info = 
  {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
    .commandPool = g_vulkan_state->command_pool,
    .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
    .commandBufferCount = 1,
  };
  
  res = vkAllocateCommandBuffers(g_vulkan_state->device, &alloc_info, &g_vulkan_state->command_buffer);
  if (res != VK_SUCCESS) 
  {
    log_error("failed to allocate Vulkan command buffer");
    return;
  }
  
  // tec: descriptor pool
  VkDescriptorPoolSize pool_sizes[] = 
  {
    { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 32 },
  };
  
  VkDescriptorPoolCreateInfo desc_pool_info = 
  {
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
    .poolSizeCount = 1,
    .pPoolSizes = pool_sizes,
    .maxSets = 32,
  };
  
  res = vkCreateDescriptorPool(g_vulkan_state->device, &desc_pool_info, 0, &g_vulkan_state->descriptor_pool);
  if (res != VK_SUCCESS) 
  {
    log_error("failed to create Vulkan descriptor pool");
    return;
  }
  
  ProfEnd();
}

internal void 
gpu_release(void)
{
  arena_release(g_vulkan_state->arena);
}

internal U64 gpu_device_total_memory(void)
{
  return 0;
}
internal U64 gpu_device_free_memory(void)
{
  return 0;
}

internal U64 gpu_hash_from_string(String8 str)
{
  return 0;
}
internal String8 gpu_get_device_id_string(Arena* arena)
{
  
}
internal String8 gpu_get_kernel_cache_path(Arena* arena, String8 source, String8 kernel_name)
{
  
}

internal GPU_Buffer*
gpu_buffer_alloc(U64 size, GPU_BufferFlags flags, void* data)
{
  GPU_Buffer* result = push_array(g_vulkan_state->arena, GPU_Buffer, 1);
  result->size = size;
  result->mapped_ptr = 0;
  
  VkBufferUsageFlags usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
  
  VkBufferCreateInfo buf_info = 
  {
    .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
    .size = size,
    .usage = usage,
    .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
  };
  
  VkResult res = vkCreateBuffer(g_vulkan_state->device, &buf_info, 0, &result->buffer);
  if (res != VK_SUCCESS)
  {
    log_error("failed to create Vulkan buffer");
    return 0;
  }
  
  VkMemoryRequirements mem_req;
  vkGetBufferMemoryRequirements(g_vulkan_state->device, result->buffer, &mem_req);
  
  VkMemoryPropertyFlags mem_props = 0;
  B32 host_visible = (flags & GPU_BufferFlag_HostVisible) != 0;
  B32 device_local = (flags & GPU_BufferFlag_DeviceLocal) != 0;
  
  if (host_visible)
  {
    mem_props |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
  }
  
  if (device_local)
  {
    mem_props |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
  }
  
  U32 mem_type = gpu_vulkan_find_memory_type(mem_req.memoryTypeBits, mem_props);
  if (mem_type == UINT32_MAX)
  {
    log_error("No matching memory type for Vulkan buffer.");
    return 0;
  }
  
  VkMemoryAllocateInfo alloc_info = 
  {
    .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
    .allocationSize = mem_req.size,
    .memoryTypeIndex = mem_type,
  };
  
  res = vkAllocateMemory(g_vulkan_state->device, &alloc_info, 0, &result->memory);
  if (res != VK_SUCCESS)
  {
    log_error("Failed to allocate Vulkan buffer memory.");
    return 0;
  }
  
  vkBindBufferMemory(g_vulkan_state->device, result->buffer, result->memory, 0);
  
  if (host_visible)
  {
    vkMapMemory(g_vulkan_state->device, result->memory, 0, size, 0, &result->mapped_ptr);
    if (data) 
    {
      MemoryCopy(result->mapped_ptr, data, size);
    }
  }
  else if (data)
  {
    log_error("Can't upload data to non-host-visible Vulkan buffer yet.");
  }
  
  return result;
}

internal void
gpu_buffer_release(GPU_Buffer* buffer)
{
  if (buffer->mapped_ptr)
  {
    vkUnmapMemory(g_vulkan_state->device, buffer->memory);
  }
  
  vkDestroyBuffer(g_vulkan_state->device, buffer->buffer, 0);
  vkFreeMemory(g_vulkan_state->device, buffer->memory, 0);
}

internal void
gpu_buffer_write(GPU_Buffer* buffer, void* data, U64 size)
{
  if (!buffer->mapped_ptr)
  {
    log_error("gpu_buffer_write: buffer is not host-visible.");
    return;
  }
  
  if (size > buffer->size)
  {
    log_error("gpu_buffer_write: write size exceeds buffer size.");
    return;
  }
  
  MemoryCopy(buffer->mapped_ptr, data, size);
}

internal void
gpu_buffer_read(GPU_Buffer* buffer, void* data, U64 size)
{
  f (!buffer->mapped_ptr)
  {
    log_error("gpu_buffer_read: buffer is not host-visible.");
    return;
  }
  
  if (size > buffer->size)
  {
    log_error("gpu_buffer_read: read size exceeds buffer size.");
    return;
  }
  
  MemoryCopy(data, buffer->mapped_ptr, size);
}

internal String8
gpu_vulkan_load_or_build_spirv(String8 source_glsl, String8 kernel_name)
{
  ProfBeginFunction();
  Temp scratch = scratch_begin(0, 0);
  
  String8 cache_path = gpu_get_kernel_cache_path(scratch.arena, source_glsl, kernel_name);
  String8 result = {0};
  
#if !FORCE_KERNEL_COMPILATION
  if (os_file_path_exists(cache_path))
  {
    OS_Handle file = os_file_open(OS_AccessFlag_Read, cache_path);
    U64 file_size = os_properties_from_file(file).size;
    if (file_size > 0)
    {
      result = os_string_from_file_range(g_vulkan_state->arena, file, r1u64(0, file_size));
    }
    os_file_close(file);
    
    if (result.size > 0)
    {
      log_info("loaded cached SPIR-V binary for kernel '%.*s'", str8_varg(kernel_name));
      scratch_end(scratch);
      ProfEnd();
      return result;
    }
  }
#endif
  
  // tec: TODO compile GLSL to SPIR-V
  String8 spirv_bin = {0};
  
  if (spirv_bin.size > 0)
  {
    OS_Handle out_file = os_file_open(OS_AccessFlag_Write, cache_path);
    if (!os_handle_match(os_handle_zero(), out_file))
    {
      os_file_write(out_file, r1u64(0, spirv_bin.size), spirv_bin.str);
      os_file_close(out_file);
    }
  }
  
  scratch_end(scratch);
  ProfEnd();
  return spirv_bin;
}

internal GPU_Kernel* 
gpu_kernel_alloc(String8 name, String8 src)
{
  ProfBeginFunction();
  Temp scratch = scratch_begin(0, 0);
  
  String8 spirv_bin = gpu_vulkan_load_or_build_spirv(glsl_src, name);
  if (spirv_bin.size == 0)
  {
    log_error("failed to load or build SPIR-V for kernel '%.*s'", str8_varg(name));
    scratch_end(scratch);
    ProfEnd();
    return 0;
  }
  
  //GPU_Kernel* kernel = gpu_kernel_create(name, spirv_bin);
  scratch_end(scratch);
  ProfEnd();
  return kernel;
}

internal void
gpu_kernel_release(GPU_Kernel *kernel)
{
  
}

internal void
gpu_kernel_set_arg_buffer(GPU_Kernel* kernel, U32 index, GPU_Buffer* buffer)
{
}

internal void
gpu_kernel_set_arg_u64(GPU_Kernel* kernel, U32 index, U64 value)
{
  
}

internal void
gpu_kernel_execute(GPU_Kernel* kernel, U32 global_work_size, U32 local_work_size)
{
  
}

internal String8
gpu_generate_kernel_from_ir(Arena* arena, String8 kernel_name, GDB_Database* database, IR_Node* ir_node, String8List* active_columns)
{
  return str8_zero();
}