

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
  clEnqueueWriteBuffer(g_opencl_state->command_queue, buffer->buffer, CL_TRUE, 0, size, data, 0, NULL, NULL);
}

internal void
gpu_buffer_read(GPU_Buffer* buffer, void* data, U64 size)
{
  clEnqueueReadBuffer(g_opencl_state->command_queue, buffer->buffer, CL_TRUE, 0, size, data, 0, NULL, NULL);
}

//~ tec: kernel
internal GPU_Kernel*
gpu_kernel_alloc(String8 name, String8 src)
{
  GPU_Kernel* kernel = push_array(g_opencl_state->arena, GPU_Kernel, 1);
  
  cl_int ret = 0;
  
  cl_program program = gpu_opencl_build_program(src);
  kernel->program = program;
  kernel->kernel = clCreateKernel(program, name.str, &ret);
  
  if (ret != CL_SUCCESS) 
  {
    log_error("failed to create kernel");
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
  
  for (U64 i = 0; i < table->column_count; i++)
  {
    err = clSetKernelArg(kernel->kernel, i, sizeof(cl_mem), &table->columns[i].buffer);
    if (err != CL_SUCCESS)
    {
      log_error("failed to set argument %llu for kernel (code: %d)", i, err);
    }
  }
  
  err = clEnqueueNDRangeKernel(g_opencl_state->command_queue, kernel->kernel, 1, NULL, global_size, local_size, 0, NULL, NULL);
  if (err != CL_SUCCESS)
  {
    log_error("failed to execute OpenCL kernel (Code: %d)", err);
    return;
  }
  
  // Wait for execution to complete
  clFinish(g_opencl_state->command_queue);
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
  
  // Transfer each column to the GPU
  for (U64 i = 0; i < table->column_count; i++)
  {
    GDB_Column* column = table->columns[i];
    
    // Determine buffer size
    U64 buffer_size = column->size * column->row_count;
    if (column->type == GDB_ColumnType_String8)
    {
      buffer_size = column->offsets[column->row_count - 1]; // Size of variable-width data
    }
    
    // Allocate GPU buffer
    gpu_table->columns[i].buffer = clCreateBuffer(g_opencl_state->context,
                                                  CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
                                                  buffer_size,
                                                  column->data, // Copy initial data from CPU to GPU
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

String8 g_test_kernel_string = str8_lit_comp("__kernel void test_kernel(__global int *input, const int value, __global int *output)"
                                             "{"
                                             "int gid = get_global_id(0);"
                                             "output[gid] = input[gid] + value;"
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
