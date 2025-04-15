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

internal cl_program
gpu_opencl_build_program(String8 source)
{
  cl_program program = { 0 };
  cl_int ret = 0;
  
  program = clCreateProgramWithSource(g_opencl_state->context, 1, &source.str, NULL, &ret);
  if (ret != CL_SUCCESS)
  {
    log_error("Failed to create OpenCL program.\n");
  }
  
  ret = clBuildProgram(program, 1, &g_opencl_state->device, NULL, NULL, NULL);
  if (ret != CL_SUCCESS)
  {
    log_error("failed to build OpenCL program.\n");
    char log[2048];
    clGetProgramBuildInfo(program, g_opencl_state->device, CL_PROGRAM_BUILD_LOG, sizeof(log), log, NULL);
    log_error("build log:\n%s\n", log);
  }
  
  
  return program;
}

internal void
gpu_init(void)
{
  Arena* arena = arena_alloc();
  g_opencl_state = push_array(arena, GPU_State, 1);
  g_opencl_state->arena = arena;
  
  //- tec: OpenCL setup
  cl_int ret;
  
  //- tec: get platform and device
  ret = clGetPlatformIDs(1, &g_opencl_state->platform, NULL);
  if (ret != CL_SUCCESS)
  {
    log_error("Failed to get OpenCL platform.\n");
  }
  
  ret = clGetDeviceIDs(g_opencl_state->platform, CL_DEVICE_TYPE_GPU, 1, &g_opencl_state->device, NULL);
  if (ret != CL_SUCCESS) 
  {
    log_error("Failed to get OpenCL device.\n");
  }
  
  //- tec: create context
  g_opencl_state->context = clCreateContext(NULL, 1, &g_opencl_state->device, NULL, NULL, &ret);
  if (ret != CL_SUCCESS) 
  {
    log_error("Failed to create OpenCL context.\n");
  }
  
  //- tec: create command queue
  g_opencl_state->command_queue = clCreateCommandQueue(g_opencl_state->context, g_opencl_state->device, 0, &ret);
  if (ret != CL_SUCCESS) 
  {
    log_error("Failed to create OpenCL command queue.\n");
  }
}

internal void
gpu_release(void)
{
  clReleaseCommandQueue(g_opencl_state->command_queue);
  clReleaseContext(g_opencl_state->context);
  
  arena_release(g_opencl_state->arena);
}

//~ tec: buffer
internal GPU_Buffer*
gpu_buffer_alloc(U64 size, GPU_BufferFlags flags, void* data)
{
  GPU_Buffer* buffer = push_array(g_opencl_state->arena, GPU_Buffer, 1);
  
  cl_int result = 0;
  
  cl_mem_flags cl_flags = gpu_flags_to_opencl_flags(flags);
  
  buffer->size = size;
  buffer->buffer = clCreateBuffer(g_opencl_state->context, cl_flags, size, data ? data : NULL, &result);
  
  if (result != CL_SUCCESS) 
  {
    log_error("failed to create buffer, error: %i", result);
    return NULL;
  }
  
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
  ProfCodeBegin(clEnqueueWriteBuffer);
  {
    clEnqueueWriteBuffer(g_opencl_state->command_queue, buffer->buffer, CL_TRUE, 0, size, data, 0, NULL, NULL);
    clFinish(g_opencl_state->command_queue);
  }
  ProfCodeEnd(clEnqueueWriteBuffer);
}

internal void
gpu_buffer_read(GPU_Buffer* buffer, void* data, U64 size)
{
  ProfCodeBegin(clEnqueueReadBuffer);
  {
    clEnqueueReadBuffer(g_opencl_state->command_queue, buffer->buffer, CL_TRUE, 0, size, data, 0, NULL, NULL);
    clFinish(g_opencl_state->command_queue);
  }
  ProfCodeEnd(clEnqueueReadBuffer);
}

//~ tec: kernel
internal GPU_Kernel*
gpu_kernel_alloc(String8 name, String8 src)
{
  GPU_Kernel* kernel = push_array(g_opencl_state->arena, GPU_Kernel, 1);
  
  cl_int ret = 0;
  
  cl_program program = gpu_opencl_build_program(src);
  kernel->name = name;
  kernel->program = program;
  kernel->kernel = clCreateKernel(program, name.str, &ret);
  
  if (ret != CL_SUCCESS) 
  {
    log_error("failed to create kernel \'%.*s\'", str8_varg(kernel->name));
    return NULL;
  }
  
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
  cl_int err;
  size_t global_size[] = { global_work_size };
  size_t local_size[] = { local_work_size };
  
  ProfCodeBegin(clEnqueueNDRangeKernel);
  {
    err = clEnqueueNDRangeKernel(g_opencl_state->command_queue, kernel->kernel, 1, NULL, global_size, local_size, 0, NULL, NULL);
  }
  ProfCodeEnd(clEnqueueNDRangeKernel);
  
  if (err != CL_SUCCESS)
  {
    log_error("failed to execute OpenCL kernel (Code: %d)", err);
    return;
  }
  
  // tec: wait for execution to complete
  clFinish(g_opencl_state->command_queue);
}

internal void
gpu_kernel_set_arg_buffer(GPU_Kernel* kernel, U32 index, GPU_Buffer* buffer)
{
  cl_int err = clSetKernelArg(kernel->kernel, index, sizeof(cl_mem), &buffer->buffer);
  if (err != CL_SUCCESS)
  {
    log_error("failed to set argument %u for kernel (code: %d)", index, err);
  }
}

internal void
gpu_kernel_set_arg_u64(GPU_Kernel* kernel, U32 index, U64 value)
{
  cl_int err = clSetKernelArg(kernel->kernel, index, sizeof(cl_ulong), &value);
  if (err != CL_SUCCESS)
  {
    log_error("failed to set argument %llu for kernel (code: %d)", index, err);
  }
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
              //"    printf(\"compare_size %lu | size %lu\", (ulong)compare_size, str_size);\n"
              "    if (str_size != (ulong)compare_size) return 0;\n"
              "\n"
              "    for (ulong i = 0; i < str_size; i++) {\n"
              "        if ((char)data[start + i] != (char)compare_str[i]) {\n"
              "            return 0;\n"
              "        }\n"
              "    }\n"
              "\n"
              //"printf(\"found matching str row_index %lu\\n\", row_index);\n"
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
                          (int)left->value.size, left->value.str,
                          (int)left->value.size, left->value.str,
                          (int)right->value.size, right->value.str,
                          right->value.size);
        }
        else if (str8_match(condition->value, str8_lit("contains"), StringMatchFlag_CaseInsensitive))
        {
          str8_list_pushf(arena, builder, "gpu_str_contains(%.*s_data, %.*s_offsets, i, \"%.*s\", %llu)",
                          (int)left->value.size, left->value.str,
                          (int)left->value.size, left->value.str,
                          (int)right->value.size, right->value.str,
                          right->value.size);
        }
        else
        {
          str8_list_pushf(arena, builder, "%.*s[i] ", (int)left->value.size, left->value.str);
          str8_list_pushf(arena, builder, "%.*s", (int)condition->value.size, condition->value.str);
          str8_list_pushf(arena, builder, " \"%.*s\"", (int)right->value.size, right->value.str);
        }
      }
      else
      {
        str8_list_pushf(arena, builder, "%.*s[i] %.*s %.*s", 
                        (int)left->value.size, left->value.str,
                        (int)condition->value.size, condition->value.str,
                        (int)right->value.size, right->value.str);
      }
    }
  }
  else if (condition->type == IR_NodeType_Column)
  {
    str8_list_pushf(arena, builder, "%.*s[i]", (int)condition->value.size, condition->value.str);
  }
  else if (condition->type == IR_NodeType_Literal)
  {
    str8_list_pushf(arena, builder, "\"%.*s\"", (int)condition->value.size, condition->value.str);
  }
}

internal String8
gpu_generate_kernel_from_ir(Arena* arena, String8 kernel_name, GDB_Database* database, IR_Node* select_ir_node, String8List* active_columns)
{
  String8List builder = { 0 };
  
  IR_Node* table_node = ir_node_find_child(select_ir_node, IR_NodeType_Table);
  if (!table_node)
  {
    log_error("kernel is missing a table");
    return str8_lit("");
  }
  
  B32 contains_string_column = 0;
  for (String8Node* node = active_columns->first; node != NULL; node = node->next)
  {
    String8 str = node->string;
    GDB_ColumnType column_type = ir_find_column_type(database, select_ir_node, str);
    if (column_type == GDB_ColumnType_String8)
    {
      contains_string_column = 1;
    }
  }
  
  //str8_list_push(arena, &builder, str8_lit("#pragma OPENCL EXTENSION cl_amd_printf : enable\n"));
  if (contains_string_column)
  {
    str8_list_push(arena, &builder, g_gpu_opencl_str_match_code);
    str8_list_push(arena, &builder, g_gpu_opencl_str_contains_code);
  }
  str8_list_push(arena, &builder, str8_lit("\n"));
  str8_list_push(arena, &builder, str8_lit("__kernel void "));
  str8_list_push(arena, &builder, kernel_name);
  str8_list_push(arena, &builder, str8_lit("(\n"));
  
  for (String8Node* node = active_columns->first; node != NULL; node = node->next)
  {
    String8 str = node->string;
    GDB_ColumnType column_type = ir_find_column_type(database, select_ir_node, str);
    String8 type_string = gpu_opencl_type_from_column_type(column_type);
    if (column_type == GDB_ColumnType_String8)
    {
      str8_list_pushf(arena, &builder, 
                      "__global const %.*s* %.*s_data,\n", 
                      (int)type_string.size, type_string.str,
                      (int)str.size, str.str);
      str8_list_pushf(arena, &builder, 
                      "__global const ulong* %.*s_offsets,\n",
                      (int)str.size, str.str);
    }
    else
    {
      str8_list_pushf(arena, &builder, 
                      "__global const %.*s* %.*s,\n", 
                      (int)type_string.size, type_string.str,
                      (int)str.size, str.str);
    }
  }
  
  IR_Node* where_clause = ir_node_find_child(select_ir_node, IR_NodeType_Where);
  
  str8_list_push(arena, &builder, str8_lit("__global ulong* output_indices,\n"));
  //str8_list_push(arena, &builder, str8_lit("__global atomic_ulong* output_count,\n"));
  str8_list_push(arena, &builder, str8_lit("volatile __global ulong* output_count,\n"));
  str8_list_push(arena, &builder, str8_lit("ulong row_count) {\n"));
  
  str8_list_push(arena, &builder, str8_lit("  ulong i = get_global_id(0);\n"));
  str8_list_push(arena, &builder, str8_lit("  if (i >= row_count) return;\n"));
  //str8_list_push(arena, &builder, str8_lit("  if (i <= 38) printf(\"col_1[38]=%lu\\n\", col_1_int[i]);\n"));
  //str8_list_push(arena, &builder, str8_lit("  if (i <= 40) {\n"));
  //str8_list_push(arena, &builder, str8_lit("  printf(\"%lu\\n\", col_1_int[i]);\n"));
  //str8_list_push(arena, &builder, str8_lit("  }\n"));
  
  if (where_clause)
  {
    str8_list_push(arena, &builder, str8_lit("  if ("));
    gpu_opencl_generate_where(arena, &builder, where_clause->first);
    str8_list_push(arena, &builder, str8_lit(") {\n"));
    //str8_list_push(arena, &builder, str8_lit("    ulong index = atomic_fetch_add(output_count, 1);\n"));
    str8_list_push(arena, &builder, str8_lit("    ulong index = atomic_add(output_count, 1);\n"));
    str8_list_push(arena, &builder, str8_lit("    output_indices[index] = i;\n"));
    str8_list_push(arena, &builder, str8_lit("    printf(\"output index %i| column index %i\", index, i);\n"));
    
    str8_list_push(arena, &builder, str8_lit("  }\n"));
  }
  else
  {
    str8_list_push(arena, &builder, str8_lit("  output_indices[i] = 1;\n"));
  }
  
  str8_list_push(arena, &builder, str8_lit("}\n"));
  
  return str8_list_join(arena, &builder, NULL);
}
