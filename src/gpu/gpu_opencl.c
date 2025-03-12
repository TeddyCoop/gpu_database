
internal cl_mem_flags
gpu_flags_to_opencl_flags(GPU_BufferFlags flags)
{
  cl_mem_flags result = 0;
  
  if (flags & GPU_BufferFlag_ReadOnly)  result |= CL_MEM_READ_ONLY;
  if (flags & GPU_BufferFlag_WriteOnly) result |= CL_MEM_WRITE_ONLY;
  if (flags & GPU_BufferFlag_ReadWrite) result |= CL_MEM_READ_WRITE;
  if (flags & GPU_BufferFlag_HostVisible) result |= CL_MEM_ALLOC_HOST_PTR;
  if (flags & GPU_BufferFlag_HostCached) result |= CL_MEM_USE_HOST_PTR;
  if (flags & GPU_BufferFlag_ZeroCopy) result |= CL_MEM_USE_HOST_PTR;
  
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
  }
  ProfCodeEnd(clEnqueueWriteBuffer);
}

internal void
gpu_buffer_read(GPU_Buffer* buffer, void* data, U64 size)
{
  ProfCodeBegin(clEnqueueReadBuffer);
  {
    
    clEnqueueReadBuffer(g_opencl_state->command_queue, buffer->buffer, CL_TRUE, 0, size, data, 0, NULL, NULL);
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
    log_error("failed to create kernel \'%s\'", kernel->name.str);
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

//~ tec: test
String8 g_filter_kernel_string = str8_lit_comp(
                                               "__kernel void filter_kernel(__global const ulong *input,\n"
                                               "__global ulong *output, \n"
                                               "const ulong threshold,\n "
                                               "__global ulong* result_count)\n" 
                                               "{\n"
                                               "int gid = get_global_id(0);\n"
                                               "\n"
                                               "if (input[gid] >= threshold)\n" 
                                               "{\n"
                                               "ulong index = atomic_add(result_count, 1);\n"
                                               "output[index] = input[gid];\n"
                                               "}\n"
                                               "}");

String8 g_stress_test_kernel_string = str8_lit_comp("__kernel void stress_test(\n"
                                                    "__global const ulong* col1,   // U32 Column\n"
                                                    "__global const double* col2,  // F32 Column\n"
                                                    "__global ulong* result,       // Output (filtered U32)\n"
                                                    "__global ulong* count,        // Result count\n"
                                                    "ulong threshold               // Filter condition\n"
                                                    ") {\n"
                                                    "int gid = get_global_id(0);\n"
                                                    "\n"
                                                    "// Apply some computation (e.g., filter col1 where col2 > threshold)\n"
                                                    "if (col2[gid] > (double)threshold) {\n"
                                                    "ulong index = atomic_add(count, 1);\n"
                                                    "result[index] = col1[gid];\n"
                                                    "}\n"
                                                    "}");


//~ tec: kernel generation

/*
global String8 g_gpu_opencl_str_match_code =
str8_lit_comp(
              "int gpu_strcmp(\n"
              "  __global const char* data, __global const uint* offsets, uint row_index,\n"
              "  __constant const char* compare_str, int compare_size) {\n"
              "    uint offset = offsets[row_index];\n"
              "    for (uint i = 0; i < compare_size; i++) {\n"
              "      if (data[offset + i] != compare_str[i]) {\n"
              "        return 0;\n"
              "      }\n"
              "    }\n"
              "\n"
              "  return 1;\n"
              "}\n"
              );
*/
global String8 g_gpu_opencl_str_match_code =
str8_lit_comp(
              "int gpu_strcmp(\n"
              "  __global const char* data, __global const ulong* offsets, ulong row_index,\n"
              "  __constant const char* compare_str, int compare_size) {\n"
              "    \n"
              "    ulong start = (row_index == 0) ? 0 : offsets[row_index - 1];\n"
              "\n"
              "    for (ulong i = 0; i < compare_size; i++) {\n"
              "        if ((char)data[start + i] != (char)compare_str[i]) {\n"
              "            return 0; // Mismatch\n"
              "        }\n"
              "    }\n"
              "\n"
              "    return 1; // Match\n"
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
        if (str8_match(condition->value, str8_lit("="), 0))
        {
          str8_list_pushf(arena, builder, "gpu_strcmp(%.*s_data, %.*s_offsets, i, \"%.*s\", %llu)",
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
    log_error("'select' statement is missing a table");
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
  
  if (contains_string_column)
  {
    str8_list_push(arena, &builder, g_gpu_opencl_str_match_code);
  }
  str8_list_push(arena, &builder, str8_lit("\n"));
  str8_list_push(arena, &builder, str8_lit("__kernel void "));
  str8_list_push(arena, &builder, kernel_name);
  str8_list_push(arena, &builder, str8_lit("(\n"));
  //query(\n"));
  
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
  
  // Extract WHERE clause
  IR_Node* where_clause = ir_node_find_child(select_ir_node, IR_NodeType_Where);
  
  str8_list_push(arena, &builder, str8_lit("__global ulong* output_indices,\n"));
  str8_list_push(arena, &builder, str8_lit("ulong row_count) {\n"));
  
  str8_list_push(arena, &builder, str8_lit("  ulong i = get_global_id(0);\n"));
  //str8_list_push(arena, &builder, str8_lit("ulong offset = name_offsets[i];\n printf(\"Offset: %i, First Char: %c\\n\", offset, name_data[offset]);\n"));
  //str8_list_push(arena, &builder, str8_lit("    printf(\"Thread %lu: Offset = %lu, First Char = %c\\n\", i, offset, name_data[offset]);\n"));
  str8_list_push(arena, &builder, str8_lit("  if (i >= row_count) return;\n"));
  
  
  if (where_clause)
  {
    str8_list_push(arena, &builder, str8_lit("  if ("));
    gpu_opencl_generate_where(arena, &builder, where_clause->first);
    str8_list_push(arena, &builder, str8_lit(") {\n"));
    str8_list_push(arena, &builder, str8_lit("    output_indices[i] = 1;\n"));
    str8_list_push(arena, &builder, str8_lit("  } else {\n"));
    str8_list_push(arena, &builder, str8_lit("    output_indices[i] = 0;\n"));
    str8_list_push(arena, &builder, str8_lit("  }\n"));
  }
  else
  {
    str8_list_push(arena, &builder, str8_lit("  output_indices[i] = 1;\n"));
  }
  
  str8_list_push(arena, &builder, str8_lit("}\n"));
  
  
  return str8_list_join(arena, &builder, NULL);
}
