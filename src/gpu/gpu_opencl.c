
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
    log_error("Failed to build OpenCL program.\n");
    char log[2048];
    clGetProgramBuildInfo(program, g_opencl_state->device, CL_PROGRAM_BUILD_LOG, sizeof(log), log, NULL);
    log_error("Build log:\n%s\n", log);
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
gpu_buffer_alloc(U64 size, GPU_BufferFlags flags)
{
  GPU_Buffer* buffer = push_array(g_opencl_state->arena, GPU_Buffer, 1);
  
  cl_int result = 0;
  
  cl_mem_flags cl_flags = gpu_flags_to_opencl_flags(flags);
  
  buffer->size = size;
  buffer->buffer = clCreateBuffer(g_opencl_state->context, cl_flags, size, NULL, &result);
  
  if (result != CL_SUCCESS) 
  {
    log_error("failed to create buffer");
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
gpu_kernel_execute(GPU_Kernel* kernel, GPU_Table* table, U32 global_work_size, U32 local_work_size)
{
  cl_int err;
  size_t global_size[] = { global_work_size };
  size_t local_size[] = { local_work_size };
  
  /*
  for (U64 i = 0; i < table->column_count; i++)
  {
    err = clSetKernelArg(kernel->kernel, i, sizeof(cl_mem), &table->columns[i].buffer);
    if (err != CL_SUCCESS)
    {
      log_error("failed to set argument %llu for kernel (code: %d)", i, err);
    }
  }
  */
  
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

//~ tec: table
internal GPU_Table*
gpu_table_transfer(GDB_Table* table)
{
  GPU_Table *gpu_table = push_array(g_opencl_state->arena, GPU_Table, 1);
  
  cl_int err;
  gpu_table->column_count = table->column_count;
  gpu_table->row_count = table->row_count;
  gpu_table->columns = push_array(g_opencl_state->arena, GPU_Buffer, table->column_count);
  
  for (U64 i = 0; i < table->column_count; i++)
  {
    GDB_Column* column = table->columns[i];
    
    // Determine buffer size
    U64 buffer_size = column->size * column->row_count;
    if (column->type == GDB_ColumnType_String8)
    {
      buffer_size = column->offsets[column->row_count - 1];
    }
    
    gpu_table->columns[i].buffer = clCreateBuffer(g_opencl_state->context,
                                                  CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
                                                  buffer_size,
                                                  column->data,
                                                  &err);
    
    if (err != CL_SUCCESS)
    {
      printf("Error: Failed to allocate GPU buffer for column %llu (Code: %d)\n", i, err);
      return NULL;
    }
    
    gpu_table->columns[i].size = buffer_size;
  }
  
  return gpu_table;
}

internal void
gpu_table_release(GPU_Table* gpu_table)
{
  for (U64 i = 0; i < gpu_table->column_count; i++)
  {
    if (gpu_table->columns[i].buffer)
    {
      clReleaseMemObject(gpu_table->columns[i].buffer);
    }
  }
  // tec: TODO add to free list
}

internal GPU_Buffer*
gpu_table_get_column_buffer(GPU_Table* table, U64 column_index)
{
  return &table->columns[column_index];
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

internal void
execute_filter_kernel(GPU_Kernel* kernel, GPU_Table* gpu_table, U64 threshold, GPU_Buffer* result_buffer, GPU_Buffer* result_count)
{
  cl_int err = 0;
  cl_command_queue queue = g_opencl_state->command_queue;
  
  // Assume column 0 is the target column
  cl_mem input_buffer = gpu_table->columns[0].buffer;
  
  if (err != CL_SUCCESS) 
  {
    printf("Error: Failed to create result count buffer (Code: %d)\n", err);
    return;
  }
  
  if (!input_buffer || !result_buffer->buffer || !result_count->buffer) {
    printf("Error: One or more GPU buffers are NULL before execution.\n");
  }
  
  
  // Set kernel arguments
  err = clSetKernelArg(kernel->kernel, 0, sizeof(cl_mem), &input_buffer);
  if (err != CL_SUCCESS)
  {
    printf("Error: Failed to set kernel argument 0 (Code: %i)\n", err);
    return;
  }
  
  err = clSetKernelArg(kernel->kernel, 1, sizeof(cl_mem), &result_buffer->buffer);
  if (err != CL_SUCCESS)
  {
    printf("Error: Failed to set kernel argument 1 (Code: %i)\n", err);
    return;
  }
  
  err = clSetKernelArg(kernel->kernel, 2, sizeof(cl_ulong), &threshold);
  if (err != CL_SUCCESS)
  {
    printf("Error: Failed to set kernel argument 2 (Code: %i)\n", err);
    return;
  }
  
  err = clSetKernelArg(kernel->kernel, 3, sizeof(cl_mem), &result_count->buffer);
  if (err != CL_SUCCESS)
  {
    printf("Error: Failed to set kernel argument 3 (Code: %i)\n", err);
    return;
  }
  
  // Execute kernel
  size_t global_work_size = gpu_table->row_count;
  err = clEnqueueNDRangeKernel(queue, kernel->kernel, 1, NULL, &global_work_size, NULL, 0, NULL, NULL);
  if (err != CL_SUCCESS)
  {
    printf("Error: Failed to execute filter kernel (Code: %d)\n", err);
    return;
  }
  
}

//~ tec: kernel generation
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

internal void generate_condition_code(Arena *arena, IR_Node *node, String8List *builder) {
  if (!node) return;
  
  if (node->type == IR_NodeType_Operator)
  {
    str8_list_push(arena, builder, str8_lit("("));
    generate_condition_code(arena, node->left, builder);
    str8_list_push(arena, builder, str8_lit(" "));
    str8_list_push(arena, builder, node->value);
    str8_list_push(arena, builder, str8_lit(" "));
    generate_condition_code(arena, node->right, builder);
    str8_list_push(arena, builder, str8_lit(")"));
  } 
  else if (node->type == IR_NodeType_Column) 
  {
    // Translate column to input buffer reference
    //String8 column_buffer = get_column_buffer_index(node);  // Function to get buffer index as a String8
    String8 format = push_str8f(arena, "input_buffer_%.*s[id]", node->value.size, node->value.str);
    //String8 format = push_str8f(arena, "input_buffer_something[id]");
    str8_list_push(arena, builder, format);
  }
  else if (node->type == IR_NodeType_Literal)
  {
    // Add literal value
    str8_list_push(arena, builder, node->value);
  } 
  else if (node->type == IR_NodeType_Condition)
  {
    generate_condition_code(arena, node->left, builder);
  }
}

/*
internal String8
gpu_generate_kernel_from_ir(Arena* arena, IR_Query* query, GDB_Column** selected_columns)
{
  String8List builder = { 0 };
  
  str8_list_push(arena, &builder, str8_lit("__kernel void query("));
  
  IR_Node *column = query->select_columns;
  U32 column_index = 0;
  while (column)
  {
    str8_list_push(arena, &builder, str8_lit("__global "));
    str8_list_push(arena, &builder, gpu_opencl_type_from_column_type(selected_columns[column_index]->type));
    //str8_list_pushf(arena, &builder, "input_buffer_%u", column_index);
    str8_list_pushf(arena, &builder, "* input_buffer_%.*s", column->value.size, column->value.str);
    
    if (column->next || query->where_conditions) 
    {
      str8_list_push(arena, &builder, str8_lit(", "));
    }
    
    column = column->next;
    column_index++;
  }
  
  str8_list_push(arena, &builder, str8_lit("__global uint *output_buffer"));
  //str8_list_push(arena, &builder, str8_lit("__global uint *output_buffer, "));
  //str8_list_push(arena, &builder, str8_lit("const ulong input_buffer_size"));
  
  str8_list_push(arena, &builder, str8_lit(") {\n"));
  
  str8_list_push(arena, &builder, str8_lit("    ulong id = get_global_id(0);\n"));
  //str8_list_push(arena, &builder, str8_lit("    if (id >= input_buffer_size) return;\n"));
  
  if (query->where_conditions)
  {
    str8_list_push(arena, &builder, str8_lit("    if (!"));
    generate_condition_code(arena, query->where_conditions, &builder);
    str8_list_push(arena, &builder, str8_lit(") return;\n"));
  }
  
  str8_list_push(arena, &builder, str8_lit("    // Output selected columns\n"));
  column = query->select_columns;
  column_index = 0;
  while (column)
  {
    str8_list_pushf(arena, &builder, "    output_buffer[id * %u + %u] = input_buffer_%.*s[id];\n",
                    query->select_column_count, column_index, column->value.size, column->value.str);
    column = column->next;
    column_index++;
  }
  
  str8_list_push(arena, &builder, str8_lit("}\n"));
  
  return str8_list_join(arena, &builder, NULL);
}
*/

internal String8
gpu_generate_kernel_from_ir(Arena* arena, IR_Query* query, GDB_Column** selected_columns)
{
  String8List builder = { 0 };
  
  // Kernel header
  str8_list_push(arena, &builder, str8_lit("__kernel void query("));
  
  // Input buffers for selected columns
  IR_Node* column = query->select_columns;
  U32 column_index = 0;
  while (column)
  {
    str8_list_push(arena, &builder, str8_lit("__global "));
    str8_list_push(arena, &builder, gpu_opencl_type_from_column_type(selected_columns[column_index]->type));
    str8_list_pushf(arena, &builder, "* input_buffer_%.*s", column->value.size, column->value.str);
    
    if (column->next || query->where_conditions || 1) // Ensure proper separation
    {
      str8_list_push(arena, &builder, str8_lit(", "));
    }
    
    column = column->next;
    column_index++;
  }
  
  // Output buffer and result count
  str8_list_push(arena, &builder, str8_lit("__global ulong* output_buffer, "));
  str8_list_push(arena, &builder, str8_lit("__global ulong* result_count, "));
  str8_list_push(arena, &builder, str8_lit("const ulong input_buffer_size"));
  
  // Begin kernel body
  str8_list_push(arena, &builder, str8_lit(") {\n"));
  str8_list_push(arena, &builder, str8_lit("    ulong id = get_global_id(0);\n"));
  str8_list_push(arena, &builder, str8_lit("    if (id >= input_buffer_size) return;\n"));
  
  // Initialize WHERE clause filtering
  if (query->where_conditions)
  {
    str8_list_push(arena, &builder, str8_lit("    if (!("));
    generate_condition_code(arena, query->where_conditions, &builder);
    str8_list_push(arena, &builder, str8_lit(")) return;\n"));
  }
  
  // Atomic increment to determine output index
  str8_list_push(arena, &builder, str8_lit("    ulong output_index = atomic_add(result_count, 1);\n"));
  
  // Write selected columns to output buffer
  column = query->select_columns;
  column_index = 0;
  while (column)
  {
    str8_list_pushf(arena, &builder,
                    "    output_buffer[output_index * %u + %u] = input_buffer_%.*s[id];\n",
                    query->select_column_count, column_index, column->value.size, column->value.str);
    
    column = column->next;
    column_index++;
  }
  
  // End kernel body
  str8_list_push(arena, &builder, str8_lit("}\n"));
  
  return str8_list_join(arena, &builder, NULL);
}
