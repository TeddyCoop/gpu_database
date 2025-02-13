internal void
app_execute_query(String8 sql_query)
{
  Arena* arena = arena_alloc();
  
  SQL_TokenizeResult tokenize_result = sql_tokenize_from_text(arena, sql_query);
  sql_tokens_print(tokenize_result);
  SQL_Node* sql_root = sql_parse(arena, tokenize_result.tokens, tokenize_result.count);
  //sql_print_ast(sql_root);
  IR_Query* ir_query = ir_generate_from_ast(arena, sql_root);
  ir_print_query(ir_query);
  
  GDB_Database* database = gdb_database_load(str8_lit("data/test_2_column"));
  GDB_Table* table = gdb_database_find_table(database, ir_query->from_table->value);
  
  GDB_Column** columns = push_array(arena, GDB_Column*, ir_query->select_column_count);
  
  for (U64 i = 0; i < ir_query->select_column_count; i++)
  {
    columns[i] = gdb_table_find_column(table, ir_query->select_columns[i].value);
  }
  
  String8 gpu_query = gpu_generate_kernel_from_ir(arena, ir_query, columns);
  log_info("%s", gpu_query.str);
  
  GPU_Table* gpu_table = gpu_table_transfer(table);
  GPU_Kernel* kernel = gpu_kernel_alloc(str8_lit("query"), gpu_query);
  
  for (U64 i = 0; i < ir_query->select_column_count; i++)
  {
    gpu_kernel_set_arg_buffer(kernel, i, gpu_table_get_column_buffer(gpu_table, i));
  }
  
  GPU_Buffer* result_buffer = gpu_buffer_alloc(table->row_count * sizeof(U64), GPU_BufferFlag_ReadWrite);
  GPU_Buffer* result_count = gpu_buffer_alloc(sizeof(U64), GPU_BufferFlag_ReadWrite);
  gpu_kernel_set_arg_buffer(kernel, ir_query->select_column_count + 0, result_buffer);
  gpu_kernel_set_arg_buffer(kernel, ir_query->select_column_count + 1, result_count);
  gpu_kernel_set_arg_u64(kernel, ir_query->select_column_count + 2, table->row_count);
  
  gpu_kernel_execute(kernel, gpu_table, table->row_count, 2);
  
  U64* filtered_results = push_array(arena, U64, table->row_count);
  gpu_buffer_read(result_buffer, filtered_results, table->row_count * sizeof(U64));
  
  U64 filtered_count = 0;
  gpu_buffer_read(result_count, &filtered_count, sizeof(U64));
  
  printf("Filtered Values\n");
  for (U64 i = 0; i < filtered_count; i++)
  {
    printf("%llu ", filtered_results[i]);
  }
  printf("\n");
  
  
  arena_release(arena);
}