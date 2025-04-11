/* date = January 23rd 2025 10:18 pm */

#ifndef GPU_H
#define GPU_H

#define GPU_MAX_BUFFER_SIZE MB(64)

typedef enum GPU_BufferFlags
{
  GPU_BufferFlag_ReadOnly  = (1 << 0),
  GPU_BufferFlag_WriteOnly = (1 << 1),
  GPU_BufferFlag_ReadWrite = (1 << 2),
  GPU_BufferFlag_HostVisible = (1 << 3),
  GPU_BufferFlag_DeviceLocal = (1 << 4),
  GPU_BufferFlag_HostCached = (1 << 5),
  GPU_BufferFlag_ZeroCopy = (1 << 6),
  GPU_BufferFlag_CopyHostPointer = (1 << 7),
  GPU_BufferFlag_Dynamic = (1 << 8),
  GPU_BufferFlag_COUNT,
} GPU_BufferFlags;

typedef struct GPU_State GPU_State;
typedef struct GPU_Buffer GPU_Buffer;
typedef struct GPU_Kernel GPU_Kernel;

internal void gpu_init(void);
internal void gpu_release(void);

internal GPU_Buffer* gpu_buffer_alloc(U64 size, GPU_BufferFlags flags, void* data);
internal void gpu_buffer_release(GPU_Buffer* buffer);
internal void gpu_buffer_write(GPU_Buffer* buffer, void* data, U64 size);
internal void gpu_buffer_read(GPU_Buffer* buffer, void* data, U64 size);

internal GPU_Kernel* gpu_kernel_alloc(String8 name, String8 src);
internal void gpu_kernel_release(GPU_Kernel *kernel);
internal void gpu_kernel_execute(GPU_Kernel* kernel, U32 global_work_size, U32 local_work_size);

internal String8 gpu_generate_kernel_from_ir(Arena* arena, String8 kernel_name, GDB_Database* database, IR_Node* ir_select_node, String8List* active_columns);

#endif //GPU_H
