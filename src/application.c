
internal void
app_execute_query(String8 sql_query)
{
  Arena* arena = arena_alloc();
  
  SQL_TokenizeResult tokenize_result = sql_tokenize_from_text(arena, sql_query);
  SQL_Node* sql_root = sql_parse(arena, tokenize_result.tokens, tokenize_result.count);
  IR_Query* ir_query = ir_generate_from_ast(arena, sql_root);
  //sql_tokens_print(tokenize_result);
  //sql_print_ast(sql_root);
  //ir_print_query(ir_query);
  
  
  GDB_Database* database = NULL;
  
  for (IR_Node* ir_execution_node = ir_query->execution_nodes; ir_execution_node != NULL;
       ir_execution_node = ir_execution_node->next)
  {
    switch (ir_execution_node->type)
    {
      //~ tec: cpu
      
      case IR_NodeType_Use:
      {
        IR_Node* use_ir_node = ir_node_find_child(ir_execution_node, IR_NodeType_Database);
        
        if (use_ir_node->type == IR_NodeType_Database)
        {
          String8 database_path = push_str8f(arena, "data/%.*s", (U32)use_ir_node->value.size, use_ir_node->value.str);
          database = gdb_database_load(database_path);
          gdb_add_database(database);
        }
      } break;
      
      case IR_NodeType_Create:
      {
        IR_Node* create_ir_node = ir_execution_node->first;
        
        if (create_ir_node->type == IR_NodeType_Database)
        {
          database = gdb_database_alloc(create_ir_node->value);
          gdb_add_database(database);
        }
        else if (create_ir_node->type == IR_NodeType_Table)
        {
          GDB_Table* table = gdb_table_alloc(create_ir_node->value);
          
          for (IR_Node* column_node = create_ir_node->first; column_node != 0; column_node = column_node->next)
          {
            GDB_ColumnType column_type = gdb_column_type_from_string(column_node->first->value);
            GDB_ColumnSchema column_schema = gdb_column_schema_create(column_node->value, column_type);
            gdb_table_add_column(table, column_schema);
          }
          
          gdb_database_add_table(database, table);
        }
        
      } break;
      case IR_NodeType_Insert:
      {
        // tec: table
        IR_Node* table_object = ir_node_find_child(ir_execution_node, IR_NodeType_Table);
        //IR_Node* table_object = ir_execution_node->first;
        GDB_Table* table = gdb_database_find_table(database, table_object->value);
        
        // tec: skip column defs
        IR_Node* columns_object = table_object->next;
        
        // tec: values
        IR_Node* values_object = columns_object->next;
        
        //- tec: value group
        Temp scratch = scratch_begin(0, 0);
        
        void** row_data = push_array(scratch.arena, void*, table->column_count);
        
        for (IR_Node* value_group_node = values_object->first; value_group_node != 0; value_group_node = value_group_node->next)
        {
          U64 column_index = 0;
          
          for (IR_Node* data_node = value_group_node->first; data_node != 0; data_node = data_node->next)
          {
            if (column_index >= table->column_count)
            {
              log_error("too many values in 'insert' statement");
              return;
            }
            
            GDB_Column* column = table->columns[column_index];
            String8 value_str = data_node->value;
            
            void* value_ptr = 0;
            
            switch (column->type)
            {
              case GDB_ColumnType_U32:
              {
                U32 value = (U32)u64_from_str8(value_str, 10);
                value_ptr = &value;
              } break;
              case GDB_ColumnType_U64:
              {
                U64 value = u64_from_str8(value_str, 10);
                value_ptr = &value;
              } break;
              case GDB_ColumnType_F32:
              {
                F32 value = (F32)f64_from_str8(value_str);
                value_ptr = &value;
              } break;
              case GDB_ColumnType_F64:
              {
                F64 value = f64_from_str8(value_str);
                value_ptr = &value;
              } break;
              case GDB_ColumnType_String8:
              {
                String8* value = push_array(arena, String8, 1);
                *value = value_str;
                value_ptr = value;
              } break;
              default:
              log_error("unknown column type");
              return;
            }
            
            row_data[column_index] = value_ptr;
            column_index++;
          }
          
          if (column_index != table->column_count)
          {
            log_error("mismatch in column count and value count in 'insert' statement");
            return;
          }
          
          gdb_table_add_row(table, row_data);
        }
        
        scratch_end(scratch);
        
      } break;
      case IR_NodeType_Alter:
      {
        IR_Node* table_node = ir_node_find_child(ir_execution_node, IR_NodeType_Table);
        
      } break;
      case IR_NodeType_Delete:
      {
        String8 kernel_name = str8_lit("delete_query");
        IR_Node* where_clause = ir_node_find_child(ir_execution_node, IR_NodeType_Where);
        String8List active_columns = { 0 };
        ir_create_active_column_list(arena, where_clause, &active_columns);
        
#if 0
        String8 kernel_code = gpu_generate_kernel_from_ir(arena, kernel_name, database, ir_execution_node, &active_columns);
        //log_info("%s", kernel_code.str);
        
        
        GDB_Table* table = gdb_database_find_table(database, ir_node_find_child(ir_execution_node, IR_NodeType_Table)->value);
        U64 row_count = table->row_count;
        U64 gpu_buffer_count = 0;
        GPU_Buffer** column_gpu_buffers = 0;
        GPU_Buffer* output_buffer = 0;
        GPU_Buffer* row_count_buffer = 0;
        {
          for (String8Node* node = active_columns.first; node != NULL; node = node->next)
          {
            GDB_Column* column = gdb_table_find_column(table, node->string);
            gpu_buffer_count += column->type == GDB_ColumnType_String8 ? 2 : 1;
          }
          
          column_gpu_buffers = push_array(arena, GPU_Buffer*, gpu_buffer_count);
          U32 column_index = 0;
          for (String8Node* node = active_columns.first; node != NULL; node = node->next)
          {
            GDB_Column* column = gdb_table_find_column(table, node->string);
            U64 size = row_count * column->size;
            void* data_ptr = column->data;
            column_gpu_buffers[column_index] = gpu_buffer_alloc(size, GPU_BufferFlag_ReadWrite | GPU_BufferFlag_HostCached, data_ptr);
            column_index++;
            
            if (column->type == GDB_ColumnType_String8)
            {
              U64 offset_size = row_count * sizeof(U32);
              column_gpu_buffers[column_index] = gpu_buffer_alloc(offset_size, GPU_BufferFlag_ReadWrite | GPU_BufferFlag_HostCached, column->offsets);
              column_index++;
            }
          }
          output_buffer = gpu_buffer_alloc(row_count * sizeof(U64), GPU_BufferFlag_ReadOnly, 0);
          row_count_buffer = gpu_buffer_alloc(sizeof(U64), GPU_BufferFlag_ReadOnly | GPU_BufferFlag_HostCached, &row_count);
        }
        
        GPU_Kernel* kernel = gpu_kernel_alloc(kernel_name, kernel_code);
        
        if (!kernel)
        {
          log_error("failed to alloc kernel");
          break;
        }
        
        for (U64 i = 0; i < gpu_buffer_count; i++)
        {
          gpu_kernel_set_arg_buffer(kernel, i, column_gpu_buffers[i]);
        }
        gpu_kernel_set_arg_buffer(kernel, gpu_buffer_count+0, output_buffer);
        gpu_kernel_set_arg_buffer(kernel, gpu_buffer_count+1, row_count_buffer);
        
        gpu_kernel_execute(kernel, 4, 4);
        
        
        U64* filtered_results = push_array(arena, U64, row_count);
        gpu_buffer_read(output_buffer, filtered_results, row_count * sizeof(U64));
        
        U64 filtered_count = 0;
        gpu_buffer_read(row_count_buffer, &filtered_count, sizeof(U64));
        
        //~ tec: output
        IR_Node* select_output_columns = ir_node_find_child(ir_execution_node, IR_NodeType_ColumnList);
        for (U64 i = 0; i < filtered_count; i++)
        {
          B32 should_delete = filtered_results[i];
          if (should_delete == 1)
          {
            gdb_table_remove_row(table, i);
          }
        }
#endif
      } break;
      
      case IR_NodeType_Import:
      {
        IR_Node* table_node = ir_node_find_child(ir_execution_node, IR_NodeType_Table);
        IR_Node* import_file_node = ir_node_find_child(ir_execution_node, IR_NodeType_Literal);
        
        //if (gdb_database_contains_table(database, table_node->value))
        {
          
        }
        //else
        {
          Temp scratch = scratch_begin(0, 0);
          /*
          String8 filepath = push_str8f(scratch.arena, "data/%.*s/%.*s", 
                                        (U32)database->name.size, database->name.str,
                                        (U32)import_file_node->value.size, import_file_node->value.str);
          */
          String8 filepath = push_str8f(scratch.arena, "data/%.*s", 
                                        (U32)import_file_node->value.size, import_file_node->value.str);
          GDB_Table* table = gdb_table_import_csv(database, filepath);
          table->name = push_str8_copy(table->arena, table->name);
          scratch_end(scratch);
          gdb_database_add_table(database, table);
        }
      } break;
      
      //~ tec: gpu
      case IR_NodeType_Select:
      {
        String8 kernel_name = str8_lit("select_query");
        APP_KernelResult result = app_perform_kernel(arena, kernel_name, database, ir_execution_node);
        IR_Node* select_output_columns = ir_node_find_child(ir_execution_node, IR_NodeType_ColumnList);
        GDB_Table* table = gdb_database_find_table(database, ir_node_find_child(ir_execution_node, IR_NodeType_Table)->value);
        
        log_info("result count %llu", result.count);
        for (U64 i = 0; i < result.count; i++)
        {
          U64 row_index = result.indices[i];
          log_info("row index %llu", row_index);
        }
        
        Temp scratch = scratch_begin(0, 0);
        for (U64 i = 0; i < result.count; i++)
        {
          U64 row_index = result.indices[i];
          for (IR_Node* column_node = select_output_columns->first; column_node != NULL; column_node = column_node->next)
          {
            GDB_Column* column = gdb_table_find_column(table, column_node->value);
            void* data = gdb_column_get_data(column, row_index);
            
            switch (column->type)
            {
              case GDB_ColumnType_U32:
              printf("%u ", *(U32*)data);
              break;
              case GDB_ColumnType_U64:
              printf("%llu ", *(U64*)data);
              break;
              case GDB_ColumnType_F32:
              printf("%f ", *(F32*)data);
              break;
              case GDB_ColumnType_F64:
              printf("%lf ", *(F64*)data);
              break;
              case GDB_ColumnType_String8: 
              {
                String8 str = gdb_column_get_string(scratch.arena, column, row_index);
                printf("%.*s ", str8_varg(str));
              } break;
              default:
              printf("UNKNOWN ");
              break;
            }
          }
          printf("\n");
          scratch_end(scratch);
        }
        
      } break;
      
    }
  }
  //gdb_table_export_csv(database->tables[0], str8_lit("data/output.csv"));
  
  String8 database_filepath = push_str8f(arena, "data/%.*s", (U32)database->name.size, database->name.str);
  gdb_database_save(database, database_filepath);
  
  //test_print_database(database);
  
  arena_release(arena);
}

internal APP_KernelResult
app_perform_kernel(Arena* arena, String8 kernel_name, GDB_Database* database, IR_Node* root_node)
{
  APP_KernelResult result = { 0 };
  
  IR_Node* where_clause = ir_node_find_child(root_node, IR_NodeType_Where);
  String8List active_columns = { 0 };
  ir_create_active_column_list(arena, where_clause, &active_columns);
  String8 kernel_code = gpu_generate_kernel_from_ir(arena, kernel_name, database, root_node, &active_columns);
  log_debug("%.*s", str8_varg(kernel_code));
  //return result;
  
  GPU_Kernel* kernel = gpu_kernel_alloc(kernel_name, kernel_code);
  if (!kernel)
  {
    log_error("failed to alloc kernel");
  }
  
  GDB_Table* table = gdb_database_find_table(database, ir_node_find_child(root_node, IR_NodeType_Table)->value);
  U64 largest_column_size = 0;
  U64 gpu_buffer_count = 0;
  for (String8Node* node = active_columns.first; node != NULL; node = node->next)
  {
    GDB_Column* column = gdb_table_find_column(table, node->string);
    gpu_buffer_count += column->type == GDB_ColumnType_String8 ? 2 : 1;
    largest_column_size = Max(gdb_column_get_total_size(column), largest_column_size);
  }
  
  if (largest_column_size > GPU_MAX_BUFFER_SIZE)
  {
    log_info("chunked");
    /*
    U64 chunk_count = (largest_column_size + GPU_MAX_BUFFER_SIZE - 1) / GPU_MAX_BUFFER_SIZE;
    U64 chunk_size = GPU_MAX_BUFFER_SIZE;
    //U64 rows_per_chunk = chunk_size / column->size;
    U64 rows_per_chunk = chunk_size / table->row_count;
    */
    U64 row_size = 0;
    for (String8Node* node = active_columns.first; node != NULL; node = node->next)
    {
      GDB_Column* column = gdb_table_find_column(table, node->string);
      row_size += column->size;
    }
    if (row_size == 0) row_size = 1;
    
    U64 rows_per_chunk = GPU_MAX_BUFFER_SIZE / row_size;
    if (rows_per_chunk == 0) rows_per_chunk = 1;
    
    U64 chunk_count = (table->row_count + rows_per_chunk - 1) / rows_per_chunk;
    U64 chunk_size = GPU_MAX_BUFFER_SIZE;
    
    for (U64 chunk_index = 0; chunk_index < chunk_count; chunk_index++)
    {
      U64 chunk_offset = chunk_index * chunk_size;
      U64 chunk_rows = Min(rows_per_chunk, table->row_count - (chunk_index * rows_per_chunk));
      
      GPU_Buffer** column_gpu_buffers = push_array(arena, GPU_Buffer*, gpu_buffer_count);
      U32 column_index = 0;
      
      for (String8Node* node = active_columns.first; node != NULL; node = node->next)
      {
        GDB_Column* column = gdb_table_find_column(table, node->string);
        
        if (column->type == GDB_ColumnType_String8)
        {
          GDB_StringDataChunk chunk = gdb_column_get_string_chunk(
                                                                  arena,
                                                                  column,
                                                                  r1u64(chunk_index * rows_per_chunk, Min((chunk_index + 1) * rows_per_chunk, table->row_count))
                                                                  );
          
          if (chunk.data && chunk.offsets)
          {
            column_gpu_buffers[column_index] = gpu_buffer_alloc(chunk.size, GPU_BufferFlag_Write | GPU_BufferFlag_HostCached, chunk.data);
            column_index++;
            
            column_gpu_buffers[column_index] = gpu_buffer_alloc((chunk.row_count + 1) * sizeof(U64), GPU_BufferFlag_Write | GPU_BufferFlag_HostCached, chunk.offsets);
            column_index++;
          }
        }
        else
        {
          U64 size = 0;
          void* data_ptr = gdb_column_get_data_range(
                                                     arena,
                                                     column,
                                                     r1u64(chunk_index * rows_per_chunk, Min((chunk_index + 1) * rows_per_chunk, table->row_count)),
                                                     &size
                                                     );
          
          debug_gdb_column_get_data_range(column, r1u64(0, 40));
          
          if (data_ptr)
          {
            column_gpu_buffers[column_index] = gpu_buffer_alloc(size, GPU_BufferFlag_Write | GPU_BufferFlag_CopyHostPointer, data_ptr);
            column_index++;
          }
        }
      }
      
      GPU_Buffer* output_buffer = gpu_buffer_alloc(chunk_rows * sizeof(U64), GPU_BufferFlag_Read, 0);
      U64 zero = 0;
      GPU_Buffer* result_counter_buffer = gpu_buffer_alloc(sizeof(U64), GPU_BufferFlag_ReadWrite | GPU_BufferFlag_HostCached, &zero);
      
      for (U64 i = 0; i < gpu_buffer_count; i++)
      {
        gpu_kernel_set_arg_buffer(kernel, i, column_gpu_buffers[i]);
      }
      gpu_kernel_set_arg_buffer(kernel, gpu_buffer_count + 0, output_buffer);
      gpu_kernel_set_arg_buffer(kernel, gpu_buffer_count + 1, result_counter_buffer);
      gpu_kernel_set_arg_u64(kernel,    gpu_buffer_count + 2, chunk_rows);
      
      //gpu_kernel_execute(kernel, chunk_rows, 1);
      gpu_kernel_execute(kernel, 32, 32);
      
      
      U64 result_count = 0;
      gpu_buffer_read(result_counter_buffer, &result_count, sizeof(U64));
      U64* chunk_results = push_array(arena, U64, result_count);
      gpu_buffer_read(output_buffer, chunk_results, result_count * sizeof(U64));
      
      if (result_count != 0)
      {
        if (result.indices == 0)
        {
          result.cap = result_count;
          result.indices = push_array(arena, U64, result.cap);
        }
        else if (result.count + result_count > result.cap)
        {
          result.cap = Max(result.cap * 2, result.count + result_count);
          U64* new_ptr = push_array_no_zero(arena, U64, result.cap);
          MemoryCopy(new_ptr, result.indices, result.count * sizeof(U64));
          result.indices = new_ptr;
        }
        
        MemoryCopy(result.indices + result.count, chunk_results, result_count * sizeof(U64));
        result.count += result_count;
        
        //MemoryCopy(result.indices + (result.count * sizeof(U64)), chunk_results, result_count * sizeof(U64));
        //MemoryCopy(result.indices + result.count, chunk_results, result_count * sizeof(U64));
        //result.count += result_count;
      }
      
      gpu_buffer_release(output_buffer);
      gpu_buffer_release(result_counter_buffer);
      for (U64 i = 0; i < gpu_buffer_count; i++) gpu_buffer_release(column_gpu_buffers[i]);
    }
  }
  else
  {
    log_info("non chunked");
    GPU_Buffer** column_gpu_buffers = push_array(arena, GPU_Buffer*, gpu_buffer_count);
    U32 column_index = 0;
    
    for (String8Node* node = active_columns.first; node != NULL; node = node->next)
    {
      GDB_Column* column = gdb_table_find_column(table, node->string);
      
      if (column->type == GDB_ColumnType_String8)
      {
        GDB_StringDataChunk chunk = gdb_column_get_string_chunk(arena,
                                                                column,
                                                                r1u64(0, table->row_count));
        debug_gdb_column_get_string_chunk(column, r1u64(0, table->row_count));
        if (chunk.data && chunk.offsets)
        {
          column_gpu_buffers[column_index] = gpu_buffer_alloc(chunk.size, GPU_BufferFlag_Write | GPU_BufferFlag_CopyHostPointer, chunk.data);
          column_index++;
          
          // tec: NOTE add 1 to the row count. so the last offset used for string size calculation
          column_gpu_buffers[column_index] = gpu_buffer_alloc((chunk.row_count + 1) * sizeof(U64), GPU_BufferFlag_Write | GPU_BufferFlag_CopyHostPointer, chunk.offsets);
          column_index++;
        }
        else
        {
          log_error("Failed to load string data or offsets for column: %.*s", str8_varg(column->name));
        }
      }
      else
      {
        U64 size = 0;
        void* data_ptr = gdb_column_get_data_range(arena, column, r1u64(0, table->row_count), &size);
        column_gpu_buffers[column_index] = gpu_buffer_alloc(size, GPU_BufferFlag_Write | GPU_BufferFlag_CopyHostPointer, data_ptr);
        column_index++;
      }
    }
    
    GPU_Buffer* output_buffer = gpu_buffer_alloc(table->row_count * sizeof(U64), GPU_BufferFlag_Read, 0);
    U64 zero = 0;
    GPU_Buffer* result_counter_buffer = gpu_buffer_alloc(sizeof(U64), GPU_BufferFlag_ReadWrite | GPU_BufferFlag_HostCached, &zero);
    
    
    for (U64 i = 0; i < gpu_buffer_count; i++)
    {
      gpu_kernel_set_arg_buffer(kernel, i, column_gpu_buffers[i]);
    }
    gpu_kernel_set_arg_buffer(kernel, gpu_buffer_count + 0, output_buffer);
    gpu_kernel_set_arg_buffer(kernel, gpu_buffer_count + 1, result_counter_buffer);
    gpu_kernel_set_arg_u64(kernel,    gpu_buffer_count + 2, table->row_count);
    
    U64 group_size = 32;
    U64 row_count = table->row_count;
    U64 global_size = (row_count + (group_size - 1)) & ~(group_size - 1);
    gpu_kernel_execute(kernel, global_size, group_size);
    //gpu_kernel_execute(kernel, table->row_count, table->row_count > 32 ? 32 : 1);
    
    U64 result_count = 0;
    gpu_buffer_read(result_counter_buffer, &result_count, sizeof(U64));
    U64* indices = push_array(arena, U64, result_count);
    gpu_buffer_read(output_buffer, indices, result_count * sizeof(U64));
    result.indices = indices;
    result.count = result_count;
    
    gpu_buffer_release(output_buffer);
    gpu_buffer_release(result_counter_buffer);
    for (U64 i = 0; i < gpu_buffer_count; i++)
    {
      gpu_buffer_release(column_gpu_buffers[i]);
    }
  }
  
  return result;
}