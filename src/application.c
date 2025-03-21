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
        // tec: create nodes should only have one child
        IR_Node* use_object = ir_execution_node->first;
        
        if (use_object->type == IR_NodeType_Database)
        {
          String8 database_path = push_str8f(arena, "data/%.*s", (U32)use_object->value.size, use_object->value.str);
          database = gdb_database_load(database_path);
          gdb_add_database(database);
        }
      } break;
      
      case IR_NodeType_Create:
      {
        // tec: create nodes should only have one child
        IR_Node* create_object = ir_execution_node->first;
        
        if (create_object->type == IR_NodeType_Database)
        {
          database = gdb_database_alloc(create_object->value);
          gdb_add_database(database);
        }
        else if (create_object->type == IR_NodeType_Table)
        {
          GDB_Table* table = gdb_table_alloc(create_object->value);
          
          for (IR_Node* column_node = create_object->first; column_node != 0; column_node = column_node->next)
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
        IR_Node* table_object = ir_execution_node->first;
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
      } break;
      
      //~ tec: gpu
      case IR_NodeType_Select:
      {
        String8 kernel_name = str8_lit("select_query");
        String8List active_columns = { 0 };
        IR_Node* where_clause = ir_node_find_child(ir_execution_node, IR_NodeType_Where);
        ir_create_active_column_list(arena, where_clause, &active_columns);
        String8 kernel_code = gpu_generate_kernel_from_ir(arena, kernel_name, database, ir_execution_node, &active_columns);
        //log_info("%s", kernel_code.str);
        
        //- tec: run the kernel
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
        
        IR_Node* order_clause = ir_node_find_child(ir_execution_node, IR_NodeType_OrderBy);
        if (order_clause)
        {
          IR_Node* collate_node = ir_node_find_child(order_clause, IR_NodeType_Column);
          IR_Node* sort_node = collate_node->first;
          
          if (sort_node == 0 || sort_node->type & IR_NodeType_Ascending)
          {
            
          }
          else if (sort_node->type & IR_NodeType_Descending)
          {
            
          }
        }
        
        //~ tec: output
        IR_Node* select_output_columns = ir_node_find_child(ir_execution_node, IR_NodeType_ColumnList);
        for (U64 i = 0; i < filtered_count; i++)
        {
          U64 row_index = i;
          if (filtered_results[i] == 1)
          {
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
                  U64 offset = column->offsets[row_index];
                  U64 start = column->offsets[row_index];
                  U64 end = (row_index + 1 < column->row_count) ? column->offsets[row_index + 1] : column->variable_capacity;
                  U64 length = end - start;
                  String8 str = { .str = column->data + start, .size = length };
                  printf("%.*s ", (int)str.size, str.str);
                } break;
                default:
                printf("UNKNOWN ");
                break;
              }
            }
            printf("\n");
          }
        }
        
      } break;
      
    }
  }
  
  {
    //GDB_Table* test_table = gdb_table_import_csv(str8_lit("data/Adoptable_Pets.csv"));
    //gdb_table_export_csv(test_table, str8_lit("data/pets.csv"));
  }
  
  {
    //GDB_Table* test_table = gdb_table_import_csv(str8_lit("data/Real_Estate_Sales_2001-2022_10k.csv"));
    GDB_ColumnType types[] = 
    {
      GDB_ColumnType_U64,
      GDB_ColumnType_U64,
      GDB_ColumnType_String8,
      GDB_ColumnType_String8,
      GDB_ColumnType_String8,
      GDB_ColumnType_F64,
      GDB_ColumnType_F64,
      GDB_ColumnType_F64,
      GDB_ColumnType_String8,
      GDB_ColumnType_String8,
      GDB_ColumnType_String8,
      GDB_ColumnType_String8,
      GDB_ColumnType_String8,
      GDB_ColumnType_String8,
    };
    GDB_Table* test_table = gdb_table_import_csv(str8_lit("data/Real_Estate_Sales_2001-2022.csv"), types, ArrayCount(types));
    gdb_table_export_csv(test_table, str8_lit("data/real_estate_big.csv"));
    //GDB_Table* test_table = gdb_table_import_csv(str8_lit("data/Real_Estate_Sales_2001-2022_10.csv"), types, ArrayCount(types));
    //GDB_Table* test_table = gdb_table_import_csv(str8_lit("data/Real_Estate_Sales_2001-2022_10k.csv"), types, ArrayCount(types));
    //gdb_table_export_csv(test_table, str8_lit("data/real_estate.csv"));
  }
  
  //gdb_database_add_table(database, test_table);
  //test_print_database(database);
  //String8 database_filepath = push_str8f(arena, "data/%.*s", (U32)database->name.size, database->name.str);
  //gdb_database_save(database, database_filepath);
  
  arena_release(arena);
}