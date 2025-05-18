internal cl_mem_flags
gpu_flags_to_opencl_flags(GPU_BufferFlags flags)
{
  cl_mem_flags result = 0;
  
  if (flags & GPU_BufferFlag_Read)  result |= CL_MEM_READ_ONLY;
  if (flags & GPU_BufferFlag_Write) result |= CL_MEM_WRITE_ONLY;
  if (flags & GPU_BufferFlag_ReadWrite) result |= CL_MEM_READ_WRITE;
  if (flags & GPU_BufferFlag_HostVisible) result |= CL_MEM_ALLOC_HOST_PTR;
  if (flags & GPU_BufferFlag_HostCached) result |= CL_MEM_USE_HOST_PTR;
  if (flags & GPU_BufferFlag_ZeroCopy) result |= CL_MEM_USE_HOST_PTR;
  if (flags & GPU_BufferFlag_CopyHostPointer) result |= CL_MEM_COPY_HOST_PTR;
  
  return result;
}

internal void
gpu_init(void)
{
  ProfBeginFunction();
  
  Arena* arena = arena_alloc();
  g_opencl_state = push_array(arena, GPU_State, 1);
  g_opencl_state->arena = arena;
  
  //- tec: OpenCL setup
  cl_int ret;
  
  //- tec: get platform and device
  ret = clGetPlatformIDs(1, &g_opencl_state->platform, NULL);
  if (ret != CL_SUCCESS)
  {
    log_error("Failed to get OpenCL platform.");
  }
  
  ret = clGetDeviceIDs(g_opencl_state->platform, CL_DEVICE_TYPE_GPU, 1, &g_opencl_state->device, NULL);
  if (ret != CL_SUCCESS) 
  {
    log_error("Failed to get OpenCL device.");
  }
  
  
  //- tec: create context
  g_opencl_state->context = clCreateContext(NULL, 1, &g_opencl_state->device, NULL, NULL, &ret);
  if (ret != CL_SUCCESS) 
  {
    log_error("Failed to create OpenCL context.");
  }
  
  //- tec: create command queue
  //g_opencl_state->command_queue = clCreateCommandQueue(g_opencl_state->context, g_opencl_state->device, 0, &ret);
  cl_queue_properties props[] =
  {
    CL_QUEUE_PROPERTIES, CL_QUEUE_PROFILING_ENABLE,
    0
  };
  
  g_opencl_state->command_queue = clCreateCommandQueueWithProperties(g_opencl_state->context, g_opencl_state->device, props, &ret);
  if (ret != CL_SUCCESS) 
  {
    log_error("Failed to create OpenCL command queue.");
  }
  
  ProfEnd();
}

internal void
gpu_release(void)
{
  clReleaseCommandQueue(g_opencl_state->command_queue);
  clReleaseContext(g_opencl_state->context);
  
  arena_release(g_opencl_state->arena);
}

internal void
gpu_wait(void)
{
  ProfBeginFunction();
  
  clFinish(g_opencl_state->command_queue);
  
  ProfEnd();
}

internal U64
gpu_device_total_memory(void)
{
  ProfBeginFunction();
  
  cl_int err;
  cl_ulong total_memory = 0;
  
  err = clGetDeviceInfo(g_opencl_state->device, CL_DEVICE_GLOBAL_MEM_SIZE, sizeof(cl_ulong), &total_memory, NULL);
  if (err != CL_SUCCESS)
  {
    log_error("failed to get CL_DEVICE_GLOBAL_MEM_SIZE (err %d)\n", err);
  }
  
  ProfEnd();
  return (U64)total_memory;
}

internal U64
gpu_device_free_memory(void)
{
  return 0;
}

//~ tec: kernel caching
internal U64 
gpu_hash_from_string(String8 str)
{
  U64 hash = 14695981039346656037ULL;
  for (U64 i = 0; i < str.size; i++)
  {
    hash ^= str.str[i];
    hash *= 1099511628211ULL;
  }
  return hash;
}

internal String8
gpu_get_device_id_string(Arena* arena)
{
  char vendor[128], version[128];
  clGetDeviceInfo(g_opencl_state->device, CL_DEVICE_VENDOR, sizeof(vendor), vendor, NULL);
  clGetDeviceInfo(g_opencl_state->device, CL_DRIVER_VERSION, sizeof(version), version, NULL);
  
  String8 vendor_str = str8_cstring(vendor);
  String8 version_str = str8_cstring(version);
  return push_str8f(arena, "%.*s_%.*s", str8_varg(vendor_str), str8_varg(version_str));
}

internal String8 
gpu_get_kernel_cache_path(Arena* arena, String8 source, String8 kernel_name)
{
  String8 device_id = gpu_get_device_id_string(arena);
  U64 source_hash = gpu_hash_from_string(source);
  U64 kernel_hash = gpu_hash_from_string(kernel_name);
  U64 device_hash = gpu_hash_from_string(device_id);
  
  return push_str8f(arena, "kernel_cache/%016llx_%016llx_%016llx.bin", device_hash, source_hash, kernel_hash);
}

//~ tec: buffer
internal GPU_Buffer*
gpu_buffer_alloc(U64 size, GPU_BufferFlags flags, void* data)
{
  ProfBeginFunction();
  
  GPU_Buffer* buffer = push_array(g_opencl_state->arena, GPU_Buffer, 1);
  
  cl_int result = 0;
  
  cl_mem_flags cl_flags = gpu_flags_to_opencl_flags(flags);
  
  buffer->size = size;
  buffer->buffer = clCreateBuffer(g_opencl_state->context, cl_flags, size, data ? data : NULL, &result);
  
  if (result != CL_SUCCESS) 
  {
    log_error("failed to create buffer, error: %i", result);
    ProfEnd();
    return NULL;
  }
  
  ProfEnd();
  return buffer;
}

internal void
gpu_buffer_release(GPU_Buffer* buffer)
{
  clReleaseMemObject(buffer->buffer);
  // tec: TODO add to free list
}

internal void
gpu_buffer_write(GPU_Buffer* buffer, void* data, U64 size)
{
  ProfBeginFunction();
  
  cl_event write_event;
  clEnqueueWriteBuffer(g_opencl_state->command_queue, buffer->buffer, CL_FALSE, 0, size, data, 0, NULL, &write_event);
  clWaitForEvents(1, &write_event);
  
  cl_ulong start_time = 0;
  cl_ulong end_time   = 0;
  clGetEventProfilingInfo(write_event, CL_PROFILING_COMMAND_START, sizeof(cl_ulong), &start_time, NULL);
  clGetEventProfilingInfo(write_event, CL_PROFILING_COMMAND_END,   sizeof(cl_ulong), &end_time,   NULL);
  
  log_debug("buffer write time %llu microseconds", (end_time - start_time) / 1000);
  clReleaseEvent(write_event);
  
  ProfEnd();
}

internal void
gpu_buffer_read(GPU_Buffer* buffer, void* data, U64 size)
{
  ProfBeginFunction();
  
  cl_event read_event;
  clEnqueueReadBuffer(g_opencl_state->command_queue, buffer->buffer, CL_FALSE, 0, size, data, 0, NULL, &read_event);
  clWaitForEvents(1, &read_event);
  
  cl_ulong start_time = 0;
  cl_ulong end_time   = 0;
  clGetEventProfilingInfo(read_event, CL_PROFILING_COMMAND_START, sizeof(cl_ulong), &start_time, NULL);
  clGetEventProfilingInfo(read_event, CL_PROFILING_COMMAND_END,   sizeof(cl_ulong), &end_time,   NULL);
  
  log_debug("buffer read time %llu microseconds", (end_time - start_time) / 1000);
  clReleaseEvent(read_event);
  
  ProfEnd();
}

//~ tec: kernel
internal cl_program
gpu_opencl_load_or_build_program(String8 source, String8 kernel_name)
{
  ProfBeginFunction();
  Temp scratch = scratch_begin(0, 0);
  
  cl_program program = 0;
  cl_int ret = 0;
  cl_device_id device = g_opencl_state->device;
  cl_context context = g_opencl_state->context;
  
  String8 cache_path = gpu_get_kernel_cache_path(scratch.arena, source, kernel_name);
#if !FORCE_KERNEL_COMPILATION
  if (os_file_path_exists(cache_path))
  {
    OS_Handle file = os_file_open(OS_AccessFlag_Read, cache_path);
    U64 file_size = os_properties_from_file(file).size;
    if (file_size > 0)
    {
      String8 str_data = os_string_from_file_range(scratch.arena, file, r1u64(0, file_size));
      void *binary_data = str_data.str;
      if (binary_data != NULL)
      {
        const unsigned char *binaries[] = { (const unsigned char *)binary_data };
        size_t lengths[] = { (size_t)file_size };
        program = clCreateProgramWithBinary(context, 1, &device, lengths, binaries, NULL, &ret);
        if (ret == CL_SUCCESS)
        {
          ret = clBuildProgram(program, 1, &device, NULL, NULL, NULL);
          if (ret == CL_SUCCESS)
          {
            log_info("sucessfully build program '%.*s' from cached binary", str8_varg(kernel_name));
            scratch_end(scratch);
            ProfEnd();
            return program;
          }
          else
          {
            log_error("cached kernel binary failed to build, recompiling from source");
            clReleaseProgram(program);
            program = 0;
          }
        }
      }
    }
    os_file_close(file);
  }
#endif
  
  //- tec: if loading failed, fall back to building from source
  program = clCreateProgramWithSource(context, 1, &source.str, NULL, &ret);
  if (ret != CL_SUCCESS)
  {
    log_error("failed to create OpenCL program from source");
    scratch_end(scratch);
    return 0;
  }
  
  ret = clBuildProgram(program, 1, &device, NULL, NULL, NULL);
  if (ret != CL_SUCCESS)
  {
    char log[4096];
    clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, sizeof(log), log, NULL);
    log_error("OpenCL build failed:\n%s\n", log);
    clReleaseProgram(program);
    scratch_end(scratch);
    return 0;
  }
  
  //- tec: save compiled binary
  U64 binary_size;
  clGetProgramInfo(program, CL_PROGRAM_BINARY_SIZES, sizeof(U64), &binary_size, NULL);
  
  unsigned char *binary = push_array(scratch.arena, unsigned char, binary_size);
  unsigned char *binaries[] = { binary };
  clGetProgramInfo(program, CL_PROGRAM_BINARIES, sizeof(binaries), &binaries, NULL);
  
  OS_Handle out_file = os_file_open(OS_AccessFlag_Write, cache_path);
  if (!os_handle_match(os_handle_zero(), out_file))
  {
    os_file_write(out_file, r1u64(0, binary_size), binary);
    os_file_close(out_file);
  }
  
  scratch_end(scratch);
  ProfEnd();
  return program;
}

internal GPU_Kernel*
gpu_kernel_alloc(String8 name, String8 src)
{
  ProfBeginFunction();
  
  GPU_Kernel* kernel = push_array(g_opencl_state->arena, GPU_Kernel, 1);
  
  cl_int ret = 0;
  
  cl_program program = gpu_opencl_load_or_build_program(src, name);
  kernel->name = name;
  kernel->program = program;
  kernel->kernel = clCreateKernel(program, name.str, &ret);
  
  if (ret != CL_SUCCESS) 
  {
    log_error("failed to create kernel \'%.*s\'", str8_varg(kernel->name));
    ProfEnd();
    return NULL;
  }
  
  ProfEnd();
  return kernel;
}

internal void
gpu_kernel_release(GPU_Kernel *kernel)
{
  clReleaseKernel(kernel->kernel);
  // tec: TODO add to free list
}

internal void
gpu_kernel_execute(GPU_Kernel* kernel, U32 global_work_size, U32 local_work_size)
{
  ProfBeginFunction();
  
  cl_int err;
  size_t global_size[] = { global_work_size };
  size_t local_size[]  = { local_work_size };
  
  cl_event kernel_event;
  err = clEnqueueNDRangeKernel(g_opencl_state->command_queue, kernel->kernel, 1, NULL, global_size, local_size, 0, NULL, &kernel_event);
  
  if (err != CL_SUCCESS)
  {
    log_error("failed to execute OpenCL kernel (Code: %d)", err);
    ProfEnd();
    return;
  }
  
  clWaitForEvents(1, &kernel_event);
  
  cl_ulong start_time = 0;
  cl_ulong end_time = 0;
  clGetEventProfilingInfo(kernel_event, CL_PROFILING_COMMAND_START, sizeof(cl_ulong), &start_time, NULL);
  clGetEventProfilingInfo(kernel_event, CL_PROFILING_COMMAND_END,   sizeof(cl_ulong), &end_time,   NULL);
  
  log_debug("kernel execution time %llu microseconds", (end_time - start_time) / 1000);
  clReleaseEvent(kernel_event);
  
  ProfEnd();
}

internal void
gpu_kernel_set_arg_buffer(GPU_Kernel* kernel, U32 index, GPU_Buffer* buffer)
{
  ProfBeginFunction();
  cl_int err = clSetKernelArg(kernel->kernel, index, sizeof(cl_mem), &buffer->buffer);
  if (err != CL_SUCCESS)
  {
    log_error("failed to set argument %u for kernel (code: %d)", index, err);
  }
  ProfEnd();
}

internal void
gpu_kernel_set_arg_u64(GPU_Kernel* kernel, U32 index, U64 value)
{
  ProfBeginFunction();
  cl_int err = clSetKernelArg(kernel->kernel, index, sizeof(cl_ulong), &value);
  if (err != CL_SUCCESS)
  {
    log_error("failed to set argument %llu for kernel (code: %d)", index, err);
  }
  ProfEnd();
}

//~ tec: kernel generation
global String8 g_gpu_opencl_str_match_code =
str8_lit_comp(
              "int gpu_str_match(\n"
              "  __global const char* data, __global const ulong* offsets, ulong row_index, ulong row_count,\n"
              "  __constant const char* compare_str, int compare_size) {\n"
              "\n"
              "    ulong start = offsets[row_index];\n"
              "    ulong end = offsets[row_index+1];\n"
              "    ulong str_size = end - start;\n"
              "    if (str_size != (ulong)compare_size) return 0;\n"
              "\n"
              "    for (ulong i = 0; i < str_size; i++) {\n"
              "        if ((char)data[start + i] != (char)compare_str[i]) {\n"
              "            return 0;\n"
              "        }\n"
              "    }\n"
              "\n"
              "    return 1;\n"
              "}\n"
              );

global String8 g_gpu_opencl_str_contains_code =
str8_lit_comp(
              "int gpu_str_contains(\n"
              "  __global const char* data, __global const ulong* offsets, ulong row_index,\n"
              "  __constant const char* compare_str, int compare_size) {\n"
              "    \n"
              "    ulong start = offsets[row_index];\n"
              "    ulong end   = offsets[row_index+1];\n"
              "    ulong str_size = end - start;\n"
              "    \n"
              "    if (compare_size > str_size) return 0;\n"
              "    \n"
              "    for (ulong i = 0; i <= str_size - compare_size; i++) {\n"
              "        int match = 1;\n"
              "        for (int j = 0; j < compare_size; j++) {\n"
              "            if ((char)data[start + i + j] != (char)compare_str[j]) {\n"
              "                match = 0;\n"
              "                break;\n"
              "            }\n"
              "        }\n"
              "        if (match) return 1;\n"
              "    }\n"
              "    \n"
              "    return 0;\n"
              "}\n"
              );



internal String8
gpu_opencl_type_from_column_type(GDB_ColumnType type)
{
  switch (type)
  {
    case GDB_ColumnType_U32: return str8_lit("uint"); break;
    case GDB_ColumnType_U64: return str8_lit("ulong"); break;
    case GDB_ColumnType_F32: return str8_lit("float"); break;
    case GDB_ColumnType_F64: return str8_lit("double"); break;
    case GDB_ColumnType_String8: return str8_lit("char"); break;
  }
  
  log_error("invalid GDB_ColumnType");
  return str8_lit("invalid");
}

internal void
gpu_opencl_generate_where(Arena* arena, String8List* builder, IR_Node* condition)
{
  if (!condition) return;
  
  if (condition->type == IR_NodeType_Operator)
  {
    IR_Node* left = condition->first;
    IR_Node* right = left ? left->next : 0;
    
    if (str8_match(condition->value, str8_lit("and"), StringMatchFlag_CaseInsensitive))
    {
      str8_list_push(arena, builder, str8_lit("("));
      gpu_opencl_generate_where(arena, builder, left);
      str8_list_push(arena, builder, str8_lit(" && "));
      gpu_opencl_generate_where(arena, builder, right);
      str8_list_push(arena, builder, str8_lit(")"));
    }
    else if (str8_match(condition->value, str8_lit("or"), StringMatchFlag_CaseInsensitive))
    {
      str8_list_push(arena, builder, str8_lit("("));
      gpu_opencl_generate_where(arena, builder, left);
      str8_list_push(arena, builder, str8_lit(" || "));
      gpu_opencl_generate_where(arena, builder, right);
      str8_list_push(arena, builder, str8_lit(")"));
    }
    else
    {
      if (right->type == IR_NodeType_Literal)
      {
        if (str8_match(condition->value, str8_lit("=="), 0))
        {
          str8_list_pushf(arena, builder, "gpu_str_match(%.*s_data, %.*s_offsets, i, row_count, \"%.*s\", %llu)",
                          str8_varg(left->value),
                          str8_varg(left->value),
                          str8_varg(right->value),
                          right->value.size);
        }
        else if (str8_match(condition->value, str8_lit("contains"), StringMatchFlag_CaseInsensitive))
        {
          str8_list_pushf(arena, builder, "gpu_str_contains(%.*s_data, %.*s_offsets, i, \"%.*s\", %llu)",
                          str8_varg(left->value),
                          str8_varg(left->value),
                          str8_varg(right->value),
                          right->value.size);
        }
        else
        {
          str8_list_pushf(arena, builder, "%.*s[i] ", str8_varg(left->value));
          str8_list_pushf(arena, builder, "%.*s", str8_varg(condition->value));
          str8_list_pushf(arena, builder, " \"%.*s\"", str8_varg(right->value));
        }
      }
      else
      {
        str8_list_pushf(arena, builder, "%.*s[i] %.*s %.*s", 
                        str8_varg(left->value),
                        str8_varg(condition->value),
                        str8_varg(right->value));
      }
    }
  }
  else if (condition->type == IR_NodeType_Column)
  {
    str8_list_pushf(arena, builder, "%.*s[i]", str8_varg(condition->value));
  }
  else if (condition->type == IR_NodeType_Literal)
  {
    str8_list_pushf(arena, builder, "\"%.*s\"", str8_varg(condition->value));
  }
}

internal String8
gpu_generate_kernel_from_ir(Arena* arena, String8 kernel_name, GDB_Database* database, IR_Node* ir_node, String8List* active_columns)
{
  ProfBeginFunction();
  
  String8List builder = { 0 };
  
  IR_Node* table_node = ir_node_find_child(ir_node, IR_NodeType_Table);
  if (!table_node)
  {
    log_error("kernel is missing a table");
    return str8_lit("");
  }
  
  B32 contains_string_column = 0;
  for (String8Node* node = active_columns->first; node != NULL; node = node->next)
  {
    String8 str = node->string;
    GDB_ColumnType column_type = ir_find_column_type(database, ir_node, str);
    if (column_type == GDB_ColumnType_String8)
    {
      contains_string_column = 1;
    }
  }
  
  if (contains_string_column)
  {
    str8_list_push(arena, &builder, g_gpu_opencl_str_match_code);
    str8_list_push(arena, &builder, g_gpu_opencl_str_contains_code);
    str8_list_push(arena, &builder, str8_lit("\n"));
  }
  str8_list_push(arena, &builder, str8_lit("__kernel void "));
  str8_list_push(arena, &builder, kernel_name);
  str8_list_push(arena, &builder, str8_lit("(\n"));
  
  for (String8Node* node = active_columns->first; node != NULL; node = node->next)
  {
    String8 str = node->string;
    GDB_ColumnType column_type = ir_find_column_type(database, ir_node, str);
    String8 type_string = gpu_opencl_type_from_column_type(column_type);
    if (column_type == GDB_ColumnType_String8)
    {
      str8_list_pushf(arena, &builder, 
                      "__global const %.*s* %.*s_data,\n", 
                      str8_varg(type_string),
                      str8_varg(str));
      str8_list_pushf(arena, &builder, 
                      "__global const ulong* %.*s_offsets,\n",
                      str8_varg(str));
    }
    else
    {
      str8_list_pushf(arena, &builder, 
                      "__global const %.*s* %.*s,\n", 
                      str8_varg(type_string),
                      str8_varg(str));
    }
  }
  
  IR_Node* where_clause = ir_node_find_child(ir_node, IR_NodeType_Where);
  
  str8_list_push(arena, &builder, str8_lit("__global ulong* output_indices,\n"));
  //str8_list_push(arena, &builder, str8_lit("__global atomic_ulong* output_count,\n"));
  str8_list_push(arena, &builder, str8_lit("volatile __global ulong* output_count,\n"));
  str8_list_push(arena, &builder, str8_lit("ulong row_count) {\n"));
  
  str8_list_push(arena, &builder, str8_lit("  ulong i = get_global_id(0);\n"));
  str8_list_push(arena, &builder, str8_lit("  if (i >= row_count) return;\n"));
  
  if (where_clause)
  {
    str8_list_push(arena, &builder, str8_lit("  if ("));
    gpu_opencl_generate_where(arena, &builder, where_clause->first);
    str8_list_push(arena, &builder, str8_lit(") {\n"));
    //str8_list_push(arena, &builder, str8_lit("    ulong index = atomic_fetch_add(output_count, 1);\n"));
    str8_list_push(arena, &builder, str8_lit("    ulong index = atomic_add(output_count, 1);\n"));
    str8_list_push(arena, &builder, str8_lit("    output_indices[index] = i;\n"));
    
    str8_list_push(arena, &builder, str8_lit("  }\n"));
  }
  else
  {
    str8_list_push(arena, &builder, str8_lit("  output_indices[i] = 1;\n"));
  }
  
  str8_list_push(arena, &builder, str8_lit("}"));
  
  String8 result = str8_list_join(arena, &builder, NULL);
  
  ProfEnd();
  return result;
}
