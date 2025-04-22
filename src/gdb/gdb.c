internal void
gdb_init(void)
{
  ProfBeginFunction();
  
  Arena* arena = arena_alloc(.reserve_size=GDB_STATE_ARENA_RESERVE_SIZE, .commit_size=GDB_STATE_ARENA_COMMIT_SIZE);
  g_gdb_state = push_array(arena, GDB_State, 1);
  g_gdb_state->arena = arena;
  
  g_gdb_state->databases = NULL;
  g_gdb_state->rw_mutex = os_rw_mutex_alloc();
  
  ProfEnd();
}

internal void
gdb_release(void)
{
  os_mutex_release(g_gdb_state->rw_mutex);
  arena_release(g_gdb_state->arena);
}

internal void
gdb_add_database(GDB_Database* database)
{
  ProfBeginFunction();
  
  if (g_gdb_state->database_count == 0)
  {
    g_gdb_state->databases = push_array(g_gdb_state->arena, GDB_Database*, 2);
    g_gdb_state->database_capacity = 2;
  }
  else if (g_gdb_state->database_count >= g_gdb_state->database_capacity)
  {
    U64 new_capacity = g_gdb_state->database_capacity * 2;
    GDB_Database** new_databases = push_array(g_gdb_state->arena, GDB_Database*, new_capacity);
    MemoryCopy(new_databases, g_gdb_state->databases, sizeof(GDB_Database*) * g_gdb_state->database_count);
    g_gdb_state->databases = new_databases;
    g_gdb_state->database_capacity = new_capacity;
  }
  
  g_gdb_state->databases[g_gdb_state->database_count++] = database;
  ProfEnd();
}

//~ tec: database
internal GDB_Database*
gdb_database_alloc(String8 name)
{
  Arena* arena = arena_alloc(.reserve_size=GDB_DATABASE_ARENA_RESERVE_SIZE, .commit_size=GDB_DATABASE_ARENA_COMMIT_SIZE);
  GDB_Database* database = push_array(arena, GDB_Database, 1);
  
  database->name = name;
  database->tables = NULL;
  database->arena = arena;
  
  return database;
}

internal void
gdb_database_release(GDB_Database* database)
{
  arena_release(database->arena);
}

internal void
gdb_database_add_table(GDB_Database* database, GDB_Table* table)
{
  if (database->table_count == 0)
  {
    database->tables = push_array(database->arena, GDB_Table*, 2);
    database->table_capacity = 2;
  }
  else if (database->table_count >= database->table_capacity)
  {
    U64 new_capacity = database->table_capacity * 2;
    GDB_Table** new_tables = push_array(database->arena, GDB_Table*, new_capacity);
    MemoryCopy(new_tables, database->tables, sizeof(GDB_Table*) * database->table_count);
    database->tables = new_tables;
    database->table_capacity = new_capacity;
  }
  
  table->parent_database = database;
  database->tables[database->table_count++] = table;
}

global String8 g_gdb_database_save_path = str8_lit_comp("data/");

internal B32
gdb_database_save(GDB_Database* database, String8 directory)
{
  ProfBeginFunction();
  
  Temp scratch = scratch_begin(0, 0);
  
  if (!os_make_directory(directory))
  {
    log_error("failed to create/open database directory: %s", directory.str);
    return 0;
  }
  
  for (U64 i = 0; i < database->table_count; i++)
  {
    GDB_Table* table = database->tables[i];
    String8 table_dir = push_str8f(scratch.arena, "%.*s/%.*s", str8_varg(directory), str8_varg(table->name));
    
    if (!os_make_directory(table_dir))
    {
      log_error("failed to create/open table directory: %.*s", str8_varg(table_dir));
      ProfEnd();
      return 0;
    }
    
    if (!gdb_table_save(table, table_dir))
    {
      log_error("failed to save table: %s", table->name.str);
      ProfEnd();
      return 0;
    }
  }
  scratch_end(scratch);
  
  ProfEnd();
  return 1;
}

internal GDB_Database*
gdb_database_load(String8 directory_path)
{
  ProfBeginFunction();
  
  GDB_Database* database = gdb_database_alloc(str8_lit("temp"));
  Temp scratch = scratch_begin(0, 0);
  
  // tec: check if the last character is a slash
  {
    U8 last_char = directory_path.str[directory_path.size-1];
    if (last_char == '/' || last_char == '\\')
    {
    }
    else
    {
      directory_path = push_str8_cat(scratch.arena, directory_path, str8_lit("/"));
    }
  }
  
  database->name = push_str8_copy(database->arena, str8_skip_last_slash(str8_chop_last_slash(directory_path)));
  
  OS_FileIter* it = os_file_iter_begin(scratch.arena, directory_path, OS_FileIterFlag_SkipFiles);
  U64 idx = 0;
  for(OS_FileInfo info = {0}; idx < 16384 && os_file_iter_next(scratch.arena, it, &info); idx += 1)
  {
    String8 table_dir = push_str8_cat(scratch.arena, directory_path, info.name);
    String8 meta_path = push_str8f(scratch.arena, "%.*s/%.*s.meta", str8_varg(table_dir), str8_varg(info.name));
    GDB_Table* table = gdb_table_load(table_dir, meta_path);
    if (table)
    {
      gdb_database_add_table(database, table);
    }
    else
    {
      log_error("failed to load table: %.*s", str8_varg(table_dir));
    }
  }
  
  scratch_end(scratch);
  ProfEnd();
  return database;
}

internal void
gdb_database_close(GDB_Database* database)
{
  
}

internal GDB_Table* 
gdb_database_find_table(GDB_Database* database, String8 table_name)
{
  ProfBeginFunction();
  for (U64 i = 0; i < database->table_count; i++)
  {
    GDB_Table* table = database->tables[i];
    if (str8_match(table->name, table_name, 0))
    {
      ProfEnd();
      return table;
    }
  }
  log_error("failed to find table '%.*s' in database '%.*s'", str8_varg(table_name), str8_varg(database->name));
  ProfEnd();
  return NULL;
}


internal B32
gdb_database_contains_table(GDB_Database* database, String8 table_name)
{
  ProfBeginFunction();
  
  for (U64 i = 0; i < database->table_count; i++)
  {
    if (str8_match(database->tables[i]->name, table_name, 0))
    {
      ProfEnd();
      return 1;
    }
  }
  ProfEnd();
  return 0;
}


//~ tec: tables
internal GDB_Table*
gdb_table_alloc(String8 name)
{
  Arena* arena = arena_alloc(.reserve_size=GDB_TABLE_ARENA_RESERVE_SIZE, .commit_size=GDB_TABLE_ARENA_COMMIT_SIZE);
  GDB_Table* table = push_array(arena, GDB_Table, 1);
  
  table->name = name;
  table->arena = arena;
  
  return table;
}

internal void
gdb_table_release(GDB_Table* table)
{
  arena_release(table->arena);
}

internal void
gdb_table_add_column(GDB_Table* table, GDB_ColumnSchema schema)
{
  ProfBeginFunction();
  
  if (table->column_count == 0)
  {
    table->columns = push_array(table->arena, GDB_Column*, 2);
    table->column_capacity = 2;
  }
  else if (table->column_count >= table->column_capacity)
  {
    U64 new_capacity = (U64)ceil_f64(table->column_capacity * GDB_TABLE_EXPAND_FACTOR);
    GDB_Column** new_columns = push_array(table->arena, GDB_Column*, new_capacity);
    MemoryCopy(new_columns, table->columns, sizeof(GDB_Column*) * table->column_count);
    table->columns = new_columns;
    table->column_capacity = new_capacity;
  }
  
  // tec: allocate and add the new column
  GDB_Column* column = gdb_column_alloc(schema.name, schema.type, schema.size);
  column->parent_table = table;
  table->columns[table->column_count++] = column;
  
  ProfEnd();
}

internal void
gdb_table_add_row(GDB_Table* table, void** row_data)
{
  for (U64 i = 0; i < table->column_count; ++i)
  {
    gdb_column_add_data(table->columns[i], row_data[i]);
  }
  table->row_count++;
}

internal void
gdb_table_remove_row(GDB_Table* table, U64 row_index)
{
  if (row_index >= table->row_count)
  {
    log_error("row index out of bounds: %llu", row_index);
    return;
  }
  
  for (U64 i = 0; i < table->column_count; ++i)
  {
    gdb_column_remove_data(table->columns[i], row_index);
  }
  
  table->row_count--;
}

internal B32
gdb_table_save(GDB_Table* table, String8 table_dir)
{
  ProfBeginFunction();
  
  //- tec: table meta file
  Temp scratch = scratch_begin(0, 0);
  {
    String8 meta_path = push_str8f(scratch.arena, "%.*s/%.*s.meta", str8_varg(table_dir), str8_varg(table->name));
    OS_Handle meta_file = os_file_open(OS_AccessFlag_Write, meta_path);
    if (os_handle_match(os_handle_zero(), meta_file))
    {
      log_error("Failed to open metadata file: %.*s", str8_varg(meta_path));
      return 0;
    }
    
    U64 meta_size = sizeof(U64) * 2;
    for (U64 i = 0; i < table->column_count; i++)
    {
      GDB_Column* column = table->columns[i];
      meta_size += sizeof(GDB_ColumnType) + (sizeof(U64) * 3) + column->name.size;
    }
    
    U8* meta_buffer = push_array(scratch.arena, U8, meta_size);
    U8* meta_ptr = meta_buffer;
    
    *(U64*)meta_ptr = table->column_count; meta_ptr += sizeof(U64);
    *(U64*)meta_ptr = table->row_count; meta_ptr += sizeof(U64);
    
    for (U64 i = 0; i < table->column_count; i++)
    {
      GDB_Column* column = table->columns[i];
      
      *(GDB_ColumnType*)meta_ptr = column->type; meta_ptr += sizeof(GDB_ColumnType);
      *(U64*)meta_ptr = column->size; meta_ptr += sizeof(U64);
      *(U64*)meta_ptr = column->capacity; meta_ptr += sizeof(U64);
      *(U64*)meta_ptr = column->name.size; meta_ptr += sizeof(U64);
      MemoryCopy(meta_ptr, column->name.str, column->name.size);
      meta_ptr += column->name.size;
    }
    
    os_file_write(meta_file, r1u64(0, meta_size), meta_buffer);
    os_file_close(meta_file);
  }
  scratch_end(scratch);
  
  //- tec: column files
  scratch = scratch_begin(0, 0);
  for (U64 i = 0; i < table->column_count; i++)
  {
    GDB_Column* column = table->columns[i];
    
    if (!column->is_disk_backed)
    {
      String8 column_path = push_str8f(scratch.arena, "%.*s/%.*s.dat", str8_varg(table_dir), str8_varg(column->name));
      OS_Handle file = os_file_open(OS_AccessFlag_Write, column_path);
      if (os_handle_match(os_handle_zero(), file))
      {
        log_error("Failed to open column file for saving: %.*s", str8_varg(column_path));
        break;
        //return 0;
      }
      
      if (column->type == GDB_ColumnType_String8)
      {
        U64 header_size = sizeof(U64);
        U64 data_size = column->variable_capacity;
        U64 offset_size = column->capacity * sizeof(U64);
        U64 total_size = header_size + data_size + offset_size;
        
        U8* buffer = push_array(scratch.arena, U8, total_size);
        U8* write_ptr = buffer;
        
        *(U64*)write_ptr = column->variable_capacity;
        write_ptr += sizeof(U64);
        MemoryCopy(write_ptr, column->data, column->variable_capacity);
        write_ptr += column->variable_capacity;
        MemoryCopy(write_ptr, column->offsets, column->capacity * sizeof(U64));
        
        os_file_write(file, r1u64(0, total_size), buffer);
      }
      else
      {
        U64 data_size = column->capacity * column->size;
        os_file_write(file, r1u64(0, data_size), column->data);
      }
      os_file_close(file);
    }
  }
  scratch_end(scratch);
  
  ProfEnd();
  return 1;
}


internal B32
gdb_table_export_csv(GDB_Table* table, String8 path)
{
  ProfBeginFunction();
  
  OS_Handle file = os_file_open(OS_AccessFlag_Write, path);
  if (os_handle_match(os_handle_zero(), file))
  {
    log_error("failed to open CSV file for writing: %s", path.str);
    return 0;
  }
  
  Temp scratch = temp_begin(g_gdb_state->arena);
  String8List buffer = { 0 };
  
  // tec: write column headers
  for (U64 i = 0; i < table->column_count; i++)
  {
    str8_list_push(scratch.arena, &buffer, table->columns[i]->name);
    if (i < table->column_count - 1)
    {
      str8_list_push(scratch.arena, &buffer, str8_lit(","));
    }
  }
  str8_list_push(scratch.arena, &buffer, str8_lit("\n"));
  
  // tec: write row data
  for (U64 row = 0; row < table->row_count; row++)
  {
    for (U64 col = 0; col < table->column_count; col++)
    {
      GDB_Column* column = table->columns[col];
      String8 output = {0};
      
      if (column->type == GDB_ColumnType_U64)
      {
        U64* data = (U64*)gdb_column_get_data(column, row);
        output = str8_from_u64(scratch.arena, *data, 10, 0, 0);
      }
      else if (column->type == GDB_ColumnType_F64)
      {
        F64* data = (F64*)gdb_column_get_data(column, row);
        output = str8_from_f64(scratch.arena, *data, 6);
      }
      else if (column->type == GDB_ColumnType_String8)
      {
        output = gdb_column_get_string(scratch.arena, column, row);
      }
      
      if (output.size && output.str)
      {
        str8_list_push(scratch.arena, &buffer, output);
      }
      if (col < table->column_count - 1)
      {
        str8_list_push(scratch.arena, &buffer, str8_lit(","));
      }
    }
    str8_list_push(scratch.arena, &buffer, str8_lit("\n"));
  }
  
  // tec: serialize and write to file
  String8 final_output = str8_list_join(scratch.arena, &buffer, 0);
  os_file_write(file, r1u64(0, final_output.size), final_output.str);
  
  os_file_close(file);
  temp_end(scratch);
  
  ProfEnd();
  return 1;
}

internal GDB_Table*
gdb_table_load(String8 table_dir, String8 meta_path)
{
  ProfBeginFunction();
  
  GDB_Table* table = gdb_table_alloc(str8_lit("temp"));
  
  Temp scratch = temp_begin(g_gdb_state->arena);
  String8 meta_data = os_data_from_file_path(scratch.arena, meta_path);
  if (meta_data.size == 0)
  {
    log_error("Failed to read metadata: %.*s", str8_varg(meta_path));
    return NULL;
  }
  
  U8* read_ptr = meta_data.str;
  
  table->column_count = *(U64*)read_ptr; read_ptr += sizeof(U64);
  table->row_count = *(U64*)read_ptr; read_ptr += sizeof(U64);
  
  table->columns = push_array(table->arena, GDB_Column*, table->column_count);
  
  for (U64 i = 0; i < table->column_count; i++)
  {
    GDB_Column* column = gdb_column_alloc(str8_lit("temp"), 0, 0);
    column->row_count = table->row_count;
    
    column->type = *(GDB_ColumnType*)read_ptr; read_ptr += sizeof(GDB_ColumnType);
    column->size = *(U64*)read_ptr; read_ptr += sizeof(U64);
    column->capacity = *(U64*)read_ptr; read_ptr += sizeof(U64);
    
    column->name.size = *(U64*)read_ptr; read_ptr += sizeof(U64);
    column->name.str = push_array(table->arena, U8, column->name.size);
    MemoryCopy(column->name.str, read_ptr, column->name.size);
    read_ptr += column->name.size;
    
    String8 column_path = push_str8f(scratch.arena, "%.*s/%.*s.dat", str8_varg(table_dir), str8_varg(column->name));
    OS_Handle file = os_file_open(OS_AccessFlag_Read, column_path);
    if (os_handle_match(os_handle_zero(), file))
    {
      log_error("Failed to open column file: %.*s", str8_varg(column_path));
      break;
      //return NULL;
    }
    
    FileProperties props = os_properties_from_file(file);
    
    if (props.size == 0)
    {
      log_error("%.*s contains no data", str8_varg(column_path));
      continue;
    }
    
    if (props.size > GDB_DISK_BACKED_THRESHOLD_SIZE)
    {
      column->is_disk_backed = 1;
      column->disk_path = push_str8_copy(column->arena, column_path);
      if (column->type == GDB_ColumnType_String8)
      {
        OS_Handle map = os_file_map_open(OS_AccessFlag_Read, file);
        void* mapped_ptr = os_file_map_view_open(map, OS_AccessFlag_Read, r1u64(0, sizeof(U64)));
        column->variable_capacity = *(U64*)mapped_ptr;
      }
      else
      {
        column->variable_capacity = props.size - (column->capacity * sizeof(U64));
      }
    }
    else
    {
      OS_Handle map = os_file_map_open(OS_AccessFlag_Read, file);
      void* mapped_ptr = os_file_map_view_open(map, OS_AccessFlag_Read, r1u64(0, props.size));
      
      if (column->type == GDB_ColumnType_String8)
      {
        column->variable_capacity = *(U64*)mapped_ptr;
        mapped_ptr = (U8*)mapped_ptr + sizeof(U64);
        
        column->data = push_array(table->arena, U8, column->variable_capacity);
        MemoryCopy(column->data, mapped_ptr, column->variable_capacity);
        mapped_ptr = (U8*)mapped_ptr + column->variable_capacity;
        
        column->offsets = push_array(table->arena, U64, column->capacity);
        MemoryCopy(column->offsets, mapped_ptr, column->capacity * sizeof(U64));
      }
      else
      {
        column->data = push_array(table->arena, U8, column->capacity * column->size);
        MemoryCopy(column->data, mapped_ptr, column->capacity * column->size);
      }
      
      os_file_map_view_close(map, mapped_ptr, r1u64(0, props.size));
      os_file_map_close(map);
    }
    os_file_close(file);
    table->columns[i] = column;
    column->parent_table = table;
  }
  
  table->name = push_str8_copy(table->arena, str8_skip_last_slash(table_dir));
  temp_end(scratch);
  
  ProfEnd();
  
  return table;
}

THREAD_POOL_TASK_FUNC(gdb_csv_process_chunk)
{
  ProfBegin("gdb_csv_process_chunk %llu", task_id);
  
  GDB_CSV_ThreadContext* ctx_array = (GDB_CSV_ThreadContext*)raw_task;
  GDB_CSV_ThreadContext* ctx = &ctx_array[task_id];
  
  void* mapped_base = os_file_map_view_open(ctx->map, OS_AccessFlag_Read, ctx->range);
  U8* str_data = (U8*)mapped_base + ctx->offset_within_view;
  String8 data = str8(str_data, dim_1u64(ctx->range) - ctx->offset_within_view);
  
  String8List rows = str8_split_by_string_chars(arena, data, str8_lit("\n"), 0);
  if (rows.node_count == 0)
  {
    os_file_map_view_close(ctx->map, str_data, ctx->range);
    return;
  }
  
  String8Node* row = rows.first;
  while (row)
  {
    String8List values = str8_split_by_string_chars(arena, row->string, str8_lit(","), StringSplitFlag_RespectQuotes | StringSplitFlag_KeepEmpties);
    U64 col_index = 0;
    
    for (String8Node* val = values.first; val && col_index < ctx->table->column_count; val = val->next, col_index++)
    {
      GDB_Column* column = ctx->table->columns[col_index];
      
      if (val->string.size == 0)
      {
        if (column->type == GDB_ColumnType_String8)
        {
          String8 empty_str = {0};
        }
        else if (column->type == GDB_ColumnType_F64)
        {
          F64 empty_f64 = 0.0f;
        }
        else if (column->type == GDB_ColumnType_U64)
        {
          U64 empty_u64 = 0;
        }
        else if (column->type == GDB_ColumnType_F32)
        {
          F32 empty_f32 = 0.0f;
        }
        else if (column->type == GDB_ColumnType_U32)
        {
          U32 empty_u64 = 0;
        }
      }
      else
      {
        if (column->type == GDB_ColumnType_String8)
        {
          String8* str = push_array(ctx->arena, String8, 1);
          *str = push_str8_copy(ctx->arena, val->string);
          ctx->columns[col_index].values[ctx->columns[col_index].count++] = str;
        }
        else if (column->type == GDB_ColumnType_F64)
        {
          F64* data = push_array(ctx->arena, F64, 1);
          *data = f64_from_str8(val->string);
          ctx->columns[col_index].values[ctx->columns[col_index].count++] = data;
        }
        else if (column->type == GDB_ColumnType_F32)
        {
          F32* data = push_array(ctx->arena, F32, 1);
          *data = (F32)f64_from_str8(val->string);
          ctx->columns[col_index].values[ctx->columns[col_index].count++] = data;
        }
        else if (column->type == GDB_ColumnType_U64)
        {
          U64* data = push_array(ctx->arena, U64, 1);
          *data = u64_from_str8(val->string, 10);
          ctx->columns[col_index].values[ctx->columns[col_index].count++] = data;
        }
        else if (column->type == GDB_ColumnType_U32)
        {
          U32* data = push_array(ctx->arena, U32, 1);
          //*data = (U32)u64_from_str8(val->string, 10);
          *data = (U32)u32_from_u64_saturate(u64_from_str8(val->string, 10));
          ctx->columns[col_index].values[ctx->columns[col_index].count++] = data;
        }
      }
    }
    
    row = row->next;
  }
  os_file_map_view_close(ctx->map, str_data, ctx->range);
  
  ProfEnd();
}

// tec: this function is very slow. it looks the threads are waiting most of the time 
// a problem could be with the mutex, or the chunk size could be increased
// or SIMD could be used to read CSV files
internal GDB_Table*
gdb_table_import_csv(GDB_Database* database, String8 path)
{
  ProfBeginFunction();
  
  GDB_Table* table = gdb_table_alloc(str8_lit("temp"));
  table->name = str8_chop_last_dot(str8_skip_last_slash(path));
  table->parent_database = database;
  
  Temp scratch = temp_begin(g_gdb_state->arena);
  FileProperties props = os_properties_from_file_path(path);
  OS_Handle file = os_file_open(OS_AccessFlag_Read, path);
  
  if (os_handle_match(file, os_handle_zero()))
  {
    log_error("Failed to open CSV file: %.*s", str8_varg(path));
    return NULL;
  }
  
  OS_Handle map = os_file_map_open(OS_AccessFlag_Read, file);
  U64 chunk_size = AlignUpPow2(props.size / GDB_CSV_THREAD_COUNT, 64);
  U64 header_size = 0;
  U64 bytes_read = 0;
  U64 column_count = 0;
  {
    //- tec: parse headers and infer types from the first chunk
    U64 read_size = Min(chunk_size, props.size);
    void* mapped_ptr = os_file_map_view_open(map, OS_AccessFlag_Read, r1u64(0, read_size));
    if (!mapped_ptr)
    {
      log_error("Failed to map view for CSV header.");
      os_file_map_close(map);
      os_file_close(file);
      return NULL;
    }
    
    String8 header_data = str8((U8*)mapped_ptr, read_size);
    String8List rows = str8_split_by_string_chars(scratch.arena, header_data, str8_lit("\n"), 0);
    if (rows.node_count == 0)
    {
      log_error("CSV is empty: %s", path.str);
      os_file_map_view_close(map, mapped_ptr, r1u64(0, read_size));
      os_file_map_close(map);
      os_file_close(file);
      return NULL;
    }
    
    //- tec: parse column headers
    header_size = rows.first->string.size + 1 /* include '\n' size */;
    String8List columns = str8_split_by_string_chars(scratch.arena, rows.first->string, str8_lit(","), StringSplitFlag_RespectQuotes);
    column_count = columns.node_count;
    if (column_count == 0)
    {
      log_error("No columns found in CSV: %s", path.str);
      os_file_map_view_close(map, mapped_ptr, r1u64(0, read_size));
      os_file_map_close(map);
      os_file_close(file);
      return NULL;
    }
    
    GDB_ColumnType* column_types = push_array(scratch.arena, GDB_ColumnType, column_count);
    MemorySet(column_types, GDB_ColumnType_Invalid, column_count * sizeof(GDB_ColumnType));
    
    U64 sample_size = Min(256, rows.node_count - 1);
    String8Node* row_node = rows.first->next;
    
    U64 row_index = 0;
    for (row_index = 0; row_index < sample_size && row_node; row_index++, row_node = row_node->next)
    {
      String8List values = str8_split_by_string_chars(scratch.arena, row_node->string, str8_lit(","), StringSplitFlag_RespectQuotes | StringSplitFlag_KeepEmpties);
      U64 col_index = 0;
      for (String8Node* val = values.first; val && col_index < column_count; val = val->next, col_index++)
      {
        GDB_ColumnType inferred_type = gdb_infer_column_type(val->string);
        if (inferred_type != GDB_ColumnType_Invalid)
        {
          column_types[col_index] = gdb_promote_type(column_types[col_index], inferred_type);
        }
      }
    }
    
    U64 col_index = 0;
    for (String8Node* col = columns.first; col; col = col->next, col_index++)
    {
      String8 str = push_str8_copy(table->arena, str8_skip_chop_whitespace(col->string));
      if (column_types[col_index] == GDB_ColumnType_Invalid)
      {
        column_types[col_index] = GDB_ColumnType_String8;
      }
      GDB_ColumnSchema schema = gdb_column_schema_create(str, column_types[col_index]);
      gdb_table_add_column(table, schema);
    }
  }
  
  //- tec: prepare threads to process chunks
  TP_Context* thread_pool = tp_alloc(scratch.arena, GDB_CSV_THREAD_COUNT, GDB_CSV_THREAD_COUNT, str8_lit("csv_thread_pool"));
  GDB_CSV_ThreadContext contexts[GDB_CSV_THREAD_COUNT] = { 0 };
  
  U64 total_chunks = AlignUpPow2(props.size, GDB_CSV_CHUNK_SIZE) / GDB_CSV_CHUNK_SIZE;
  log_info("total chunks %llu", total_chunks);
  Arena* scratch_arena = arena_alloc(.reserve_size=MB(256), .commit_size=MB(16));
  for (U64 chunk_offset = 0; chunk_offset < total_chunks; chunk_offset += GDB_CSV_THREAD_COUNT)
  {
    U64 start_time = os_now_microseconds();
    TP_Arena* worker_arenas = tp_arena_alloc(thread_pool);
    U64 chunk_count = Min(GDB_CSV_THREAD_COUNT, total_chunks - chunk_offset);
    log_info("starting chunks %llu-%llu", chunk_offset, chunk_offset + chunk_count);
    
    for (U64 i = 0; i < chunk_count; i++)
    {
      U64 raw_offset = header_size + (chunk_offset + i) * GDB_CSV_CHUNK_SIZE;
      U64 size = Min(GDB_CSV_CHUNK_SIZE, props.size - raw_offset);
      
      U64 aligned_offset = AlignDownPow2(raw_offset, KB(64));
      U64 offset_within_view = raw_offset - aligned_offset;
      
      U64 scan_view_size = KB(2);
      Rng1U64 scan_range = r1u64(aligned_offset, aligned_offset + offset_within_view + scan_view_size);
      
      void* scan_ptr = os_file_map_view_open(map, OS_AccessFlag_Read, scan_range);
      U8* scan_bytes = (U8*)scan_ptr + offset_within_view;
      U64 bytes_remaining = props.size - raw_offset;
      
      U64 skip = 0;
      while (skip < scan_view_size && skip < bytes_remaining)
      {
        if (scan_bytes[skip] == '\n')
        {
          skip += 1;
          break;
        }
        skip += 1;
      }
      os_file_map_view_close(map, scan_ptr, scan_range);
      
      /*
      U64 adjusted_raw_offset = raw_offset;
      if (!(chunk_offset == 0 && i == 0))
      {
        adjusted_raw_offset += skip;
      }
      */
      U64 adjusted_raw_offset = raw_offset;
      if (chunk_offset == 0 && i == 0)
      {
        adjusted_raw_offset = header_size;
      }
      else
      {
        adjusted_raw_offset += skip;
      }
      aligned_offset = AlignDownPow2(adjusted_raw_offset, KB(64));
      offset_within_view = adjusted_raw_offset - aligned_offset;
      
      U64 aligned_size = Min(GDB_CSV_CHUNK_SIZE, props.size - adjusted_raw_offset);
      
      U64 max_rows = max_U16;
      GDB_CSV_ThreadColumnData* columns = push_array(scratch_arena, GDB_CSV_ThreadColumnData, column_count);
      for (U64 i = 0; i < column_count; i++)
      {
        GDB_Column* col = table->columns[i];
        columns[i].type = col->type;
        columns[i].values = push_array(scratch_arena, void*, max_rows);
        columns[i].count = 0;
      }
      
      contexts[i] = (GDB_CSV_ThreadContext)
      {
        .map = map,
        .range = r1u64(aligned_offset, aligned_offset + aligned_size + offset_within_view),
        .table = table,
        .offset_within_view = offset_within_view,
        .columns = columns,
        .arena = arena_alloc()
      };
    }
    
    tp_for_parallel(thread_pool, worker_arenas, chunk_count, gdb_csv_process_chunk, contexts);
    
    for (U64 i = 0; i < chunk_count; i++)
    {
      GDB_CSV_ThreadContext* ctx = &contexts[i];
      for (U64 col_index = 0; col_index < table->column_count; col_index++)
      {
        GDB_CSV_ThreadColumnData* col_data = &ctx->columns[col_index];
        GDB_Column* column = table->columns[col_index];
        
        for (U64 j = 0; j < col_data->count; j++)
        {
          void* value = col_data->values[j];
          gdb_column_add_data(column, value);
        }
      }
      
      arena_release(ctx->arena);
    }
    
    tp_arena_release(&worker_arenas);
    U64 end_time = os_now_microseconds();
    log_info("finished chunks %llu-%llu in: %llu millisec", chunk_offset, chunk_offset + chunk_count, (end_time - start_time) / 1000);
    
    arena_clear(scratch_arena);
  }
  arena_release(scratch_arena);
  
  tp_release(thread_pool);
  os_file_map_close(map);
  os_file_close(file);
  temp_end(scratch);
  table->row_count = table->columns[0]->row_count;
  
  for (U64 i = 0; i < table->column_count; i++)
  {
    GDB_Column* column = table->columns[i];
    gdb_column_close(column);
  }
  
  ProfEnd();
  return table;
}

internal GDB_Column*
gdb_table_find_column(GDB_Table* table, String8 column_name)
{
  for (U64 i = 0; i < table->column_count; i++)
  {
    GDB_Column* column = table->columns[i];
    if (str8_match(column->name, column_name, 0))
    {
      return column;
    }
  }
  log_error("failed to find column '%.*s' in table '%.*s", column_name.size, column_name.str,
            table->name.size, table->name.str);
  return NULL;
}

//~ tec: column
internal GDB_Column*
gdb_column_alloc(String8 name, GDB_ColumnType type, U64 size)
{
  Arena* arena = arena_alloc(.reserve_size=GDB_COLUMN_ARENA_RESERVE_SIZE, .commit_size=GDB_COLUMN_ARENA_COMMIT_SIZE);
  GDB_Column* column = push_array(arena, GDB_Column, 1);
  
  column->name = name;
  column->type = type;
  column->size = size;
  column->arena = arena;
  
  return column;
}

internal void
gdb_column_release(GDB_Column* column)
{
  arena_release(column->arena);
}

internal void
gdb_column_open(GDB_Column* column)
{
  if (!column->is_disk_backed)
  {
    gdb_column_convert_to_disk_backed(column);
  }
}

internal void
gdb_column_close(GDB_Column* column)
{
  if (column->is_disk_backed)
  {
    os_file_close(column->file);
  }
}

internal void
gdb_column_add_data_disk_backed(GDB_Column* column, void* data)
{
  if (column->type == GDB_ColumnType_String8)
  {
    String8* str = (String8*)data;
    OS_Handle file = column->file;
    if (os_handle_match(os_handle_zero(), file))
    {
      file = os_file_open(OS_AccessFlag_Read | OS_AccessFlag_Write | OS_AccessFlag_Append, column->disk_path);
      column->file = file;
    }
    
    U64 var_cap = column->variable_capacity;
    U64 var_reserved = 0;
    os_file_read(file, r1u64(0, sizeof(U64)), &var_reserved);
    
    U64 offset_array_offset = sizeof(U64) + var_reserved;
    U64 offset_count = column->row_count;
    U64 total_offsets_size = (offset_count + 1) * sizeof(U64);
    
    if (offset_count == 0)
    {
      U64 zero = 0;
      os_file_write(file, r1u64(offset_array_offset, offset_array_offset + sizeof(U64)), &zero);
    }
    
    B32 needs_growth = (var_cap + str->size > var_reserved);
    if (needs_growth)
    {
      ProfBegin("gdb_column_add_data_disk_backed growth");
      U64 new_reserved = var_reserved * 2;
      if (new_reserved < var_cap + str->size)
      {
        new_reserved = AlignUp(var_cap + str->size + GDB_COLUMN_VARIABLE_CAPACITY_ALLOC_SIZE, 8);
      }
      
      U64 old_offset_pos = sizeof(U64) + var_reserved;
      U64 new_offset_pos = sizeof(U64) + new_reserved;
      
      Temp scratch = scratch_begin(0, 0);
      void *buffer = push_array(scratch.arena, U8, total_offsets_size);
      
      os_file_read(file, r1u64(old_offset_pos, old_offset_pos + total_offsets_size), buffer);
      os_file_write(file, r1u64(new_offset_pos, new_offset_pos + total_offsets_size), buffer);
      scratch_end(scratch);
      
      os_file_write(file, r1u64(0, sizeof(U64)), &new_reserved);
      var_reserved = new_reserved;
      U64 zero_start = sizeof(U64) + var_cap;
      U64 zero_end   = sizeof(U64) + var_reserved;
      U64 zero_size  = zero_end - zero_start;
      
      /*
      if (zero_size > 0)
      {
        Temp scratch = scratch_begin(0, 0);
        void *zero_block = push_array(scratch.arena, U8, zero_size);
        os_file_write(file, r1u64(zero_start, zero_end), zero_block);
        scratch_end(scratch);
      }
      */
      
      offset_array_offset = sizeof(U64) + var_reserved;
      
      U64 new_size = offset_array_offset + total_offsets_size;
      os_file_resize(file, new_size);
      
      ProfEnd();
    }
    //U64 string_offset = var_reserved;
    U64 string_offset = var_cap;
    os_file_write(file, r1u64(sizeof(U64) + string_offset, sizeof(U64) + string_offset + str->size), str->str);
    
    //U64 new_end_offset = (offset_count > 0) ? 0 : str->size;
    U64 new_end_offset = str->size;
    if (offset_count > 0)
    {
      U64 last_offset_pos = offset_array_offset + (offset_count - 1) * sizeof(U64);
      U64 last_offset = 0;
      os_file_read(file, r1u64(last_offset_pos, last_offset_pos + sizeof(U64)), &last_offset);
      new_end_offset = last_offset + str->size;
    }
    
    os_file_write(file,
                  r1u64(offset_array_offset + offset_count * sizeof(U64),
                        offset_array_offset + (offset_count + 1) * sizeof(U64)),
                  &new_end_offset);
    column->variable_capacity += str->size;
  }
  else
  {
    OS_Handle file = column->file;
    if (os_handle_match(os_handle_zero(), file))
    {
      file = os_file_open(OS_AccessFlag_Write | OS_AccessFlag_Append, column->disk_path);
    }
    U64 offset = column->row_count * column->size;
    os_file_write(file, r1u64(offset, offset + column->size), data);
    
    if (os_handle_match(os_handle_zero(), column->file))
    {
      os_file_close(file);
    }
  }
}

internal void
gdb_column_add_data(GDB_Column* column, void* data)
{
  if (column->type == GDB_ColumnType_String8)
  {
    String8* str = (String8*)data;
    
    if (column->is_disk_backed)
    {
      gdb_column_add_data_disk_backed(column, data);
    }
    else
    {
      //- tec: grow offsets array if needed
      if (column->row_count == column->capacity)
      {
        U64 new_capacity = (column->capacity > 0) ? column->capacity * 2 : GDB_COLUMN_EXPAND_COUNT;
        U64* new_offsets = push_array(column->arena, U64, new_capacity);
        if (column->offsets) 
        {
          MemoryCopy(new_offsets, column->offsets, column->row_count * sizeof(U64));
        }
        column->offsets = new_offsets;
        column->capacity = new_capacity;
      }
      
      //- tec: grow variable data if needed
      U64 previous_offset = (column->row_count > 0) ? column->offsets[column->row_count - 1] : 0;
      U64 required_size = previous_offset + str->size;
      if (required_size > column->variable_capacity)
      {
        U64 new_variable_capacity = (column->variable_capacity > 0) ? column->variable_capacity * 2 : GDB_COLUMN_VARIABLE_CAPACITY_ALLOC_SIZE;
        while (new_variable_capacity < required_size)
        {
          new_variable_capacity *= 2;
        }
        
        if (new_variable_capacity > GDB_DISK_BACKED_THRESHOLD_SIZE)
        {
          if (!column->is_disk_backed)
          {
            gdb_column_convert_to_disk_backed(column);
          }
          gdb_column_add_data_disk_backed(column, data);
          return;
        }
        
        U8* new_data = push_array(column->arena, U8, new_variable_capacity);
        if (column->data) 
        {
          MemoryCopy(new_data, column->data, column->variable_capacity);
        }
        column->data = new_data;
        column->variable_capacity = new_variable_capacity;
      }
      
      if (column->row_count >= column->capacity) 
      {
        log_error("offset array out of bounds: row_count=%llu capacity=%llu", column->row_count, column->capacity);
      }
      
      U64 current_offset = (column->row_count > 0) ? column->offsets[column->row_count - 1] : 0;
      MemoryCopy(column->data + current_offset, str->str, str->size);
      column->offsets[column->row_count] = current_offset + str->size;
    }
  }
  else
  {
    if (column->is_disk_backed)
    {
      gdb_column_add_data_disk_backed(column, data);
    }
    else
    {
      // tec: grow if needed
      if (column->row_count == column->capacity)
      {
        // tec: check that an absurdly large buffer is not being allocated
        if (column->capacity > (max_U64 / 2))
        {
          log_error("column capacity too large, can not allocate");
          return;
        }
        
        // tec: cap capacity
        U64 new_capacity = (column->capacity > 0) ? column->capacity * 2 : GDB_COLUMN_EXPAND_COUNT;
        if (new_capacity > column->capacity + GDB_COLUMN_MAX_GROW_BY_SIZE)
        {
          new_capacity = column->capacity + GDB_COLUMN_MAX_GROW_BY_SIZE;
        }
        //log_debug("growing column: old_capacity=%llu, new_capacity=%llu, size=%llu", column->capacity, new_capacity, column->size);
        
        U8* new_data = arena_push(column->arena, new_capacity * column->size, 8);
        if (new_data == 0)
        {
          log_error("failed to allocate memory in arena");
          return;
        }
        
        if (column->capacity > 0 && column->data)
        {
          MemoryCopy(new_data, column->data, column->capacity * column->size);
        }
        column->data = new_data;
        column->capacity = new_capacity;
      }
      
      // tec: add data
      MemoryCopy(column->data + column->row_count * column->size, data, column->size);
      
      if ((column->row_count + 1) * column->size > GDB_DISK_BACKED_THRESHOLD_SIZE)
      {
        gdb_column_convert_to_disk_backed(column);
      }
    }
  }
  column->row_count++;
}

internal void
gdb_column_remove_data(GDB_Column* column, U64 row_index)
{
  if (row_index >= column->row_count)
  {
    log_error("column row index out of bounds: %llu", row_index);
    return;
  }
  
  if (column->is_disk_backed)
  {
    log_error("removing data from disk-backed column is not supported");
    return;
  }
  
  if (column->type == GDB_ColumnType_String8)
  {
    U64 start_offset = column->offsets[row_index];
    U64 end_offset = column->offsets[row_index + 1];
    U64 size_to_move = column->variable_capacity - end_offset;
    
    MemoryCopy(column->data + start_offset, column->data + end_offset, size_to_move);
    
    for (U64 i = row_index + 1; i < column->row_count; ++i)
    {
      column->offsets[i] = column->offsets[i + 1] - (end_offset - start_offset);
    }
  }
  else
  {
    U64 size_to_move = (column->row_count - row_index - 1) * column->size;
    MemoryCopy(column->data + row_index * column->size, column->data + (row_index + 1) * column->size, size_to_move);
  }
  
  column->row_count--;
}

internal void*
gdb_column_get_data(GDB_Column* column, U64 index)
{
  if (index >= column->row_count)
  {
    log_error("index %llu out of bounds %llu", index, column->row_count);
    return NULL;
  }
  
  if (column->is_disk_backed)
  {
    U64 offset = index * column->size;
    OS_Handle file = column->file;
    if (os_handle_match(os_handle_zero(), file))
    {
      file = os_file_open(OS_AccessFlag_Read, column->disk_path);
    }
    void* data = arena_push(column->arena, column->size, 8);
    os_file_read(file, r1u64(offset, offset + column->size), data);
    os_file_close(file);
    return data;
  }
  else
  {
    return (void*)(column->data + index * column->size);
  }
}

internal String8
gdb_column_get_string(Arena* arena, GDB_Column* column, U64 index)
{
  ProfBeginFunction();
  
  String8 result = { 0 };
  
  if (index >= column->row_count)
  {
    return result;
  }
  
  if (column->type == GDB_ColumnType_String8)
  {
    if (column->is_disk_backed)
    {
      OS_Handle file = column->file;
      if (os_handle_match(os_handle_zero(), file))
      {
        file = os_file_open(OS_AccessFlag_Read, column->disk_path);
      }
      
      U64 variable_reserved = 0;
      os_file_read(file, r1u64(0, sizeof(U64)), &variable_reserved); 
      
      U64 offset_base = sizeof(U64) + variable_reserved;
      U64 offset_position = offset_base + (index * sizeof(U64));
      
      U64 start_offset = 0, end_offset = 0;
      
      if (index > 0)
      {
        os_file_read(file, r1u64(offset_position - sizeof(U64), offset_position), &start_offset);
      }
      os_file_read(file, r1u64(offset_position, offset_position + sizeof(U64)), &end_offset);
      
      U64 size = end_offset - start_offset;
      result.str = arena_push(arena, size, 8);
      
      U64 data_start = sizeof(U64) + start_offset;
      os_file_read(file, r1u64(data_start, data_start + size), result.str);
      result.size = size;
      
      if (os_handle_match(os_handle_zero(), column->file))
      {
        os_file_close(file);
      }
    }
    else
    {
      U64 start = (index > 0) ? column->offsets[index - 1] : 0;
      U64 end = column->offsets[index];
      
      if (start < end && end <= column->variable_capacity)
      {
        result.str = column->data + start;
        result.size = end - start;
      }
    }
  }
  
  ProfEnd();
  return result;
}

internal U64
gdb_column_get_total_size(GDB_Column* column)
{
  U64 total_size = 0;
  
  if (column->is_disk_backed)
  {
    FileProperties props = os_properties_from_file_path(column->disk_path);
    total_size = props.size;
  }
  else
  {
    if (column->type == GDB_ColumnType_String8)
    {
      total_size += sizeof(U64);
      total_size += column->variable_capacity;
      total_size += column->row_count * sizeof(U64);
    }
    else
    {
      total_size = column->row_count * column->size;
    }
  }
  
  return total_size;
}

internal void*
gdb_column_get_data_range(Arena* arena, GDB_Column* column, Rng1U64 row_range, U64* out_size)
{
  ProfBeginFunction();
  
  U64 row_count = row_range.max + 1 - row_range.min;
  U64 size = row_count * column->size;
  
  // tec: this is probably not needed, remove
  if (column->type == GDB_ColumnType_String8)
  {
    if (column->is_disk_backed)
    {
      OS_Handle file = os_file_open(OS_AccessFlag_Read, column->disk_path);
      if (os_handle_match(os_handle_zero(), file))
      {
        log_error("failed to open disk-backed column: %.*s", str8_varg(column->disk_path));
        *out_size = 0;
        ProfEnd();
        return NULL;
      }
      
      U64 start_offset = 0;
      U64 end_offset = 0;
      U64 offset_position_start = column->variable_capacity + (row_range.min * sizeof(U64));
      U64 offset_position_end = column->variable_capacity + (row_range.max * sizeof(U64));
      
      FileProperties props = os_properties_from_file(file);
      if (offset_position_end + sizeof(U64) > props.size)
      {
        log_error("attempting to read beyond file size: offset=%llu, file_size=%llu", offset_position_end, props.size);
        os_file_close(file);
        *out_size = 0;
        ProfEnd();
        return NULL;
      }
      
      if (row_range.min == 0)
      {
        start_offset = 0;
      }
      else
      {
        if (os_file_read(file, r1u64(offset_position_start - sizeof(U64), offset_position_start), &start_offset) != sizeof(U64))
        {
          log_error("failed to read start offset for row %llu", row_range.min);
          os_file_close(file);
          *out_size = 0;
          ProfEnd();
          return NULL;
        }
      }
      
      if (row_range.max == column->row_count)
      {
        end_offset = column->variable_capacity;
      }
      else
      {
        if (os_file_read(file, r1u64(offset_position_end, offset_position_end + sizeof(U64)), &end_offset) != sizeof(U64))
        {
          log_error("failed to read end offset for row %llu", row_range.max);
          os_file_close(file);
          *out_size = 0;
          ProfEnd();
          return NULL;
        }
      }
      
      size = end_offset - start_offset;
      void* data_ptr = push_array(arena, U8, size);
      if (os_file_read(file, r1u64(start_offset, start_offset + size), data_ptr) != size)
      {
        log_error("Failed to read string data for rows [%llu - %llu]", row_range.min, row_range.max);
        os_file_close(file);
        *out_size = 0;
        ProfEnd();
        return NULL;
      }
      
      os_file_close(file);
      *out_size = size;
      ProfEnd();
      return data_ptr;
    }
    else
    {
      U64 start_offset = (row_range.min > 0) ? column->offsets[row_range.min - 1] : 0;
      U64 end_offset = column->offsets[row_range.max - 1];
      size = end_offset - start_offset;
      
      void* data_ptr = column->data + start_offset;
      *out_size = size;
      ProfEnd();
      return data_ptr;
    }
  }
  else
  {
    if (column->is_disk_backed)
    {
      void* data_ptr = push_array(arena, U8, size);
      OS_Handle file = os_file_open(OS_AccessFlag_Read, column->disk_path);
      if (!os_handle_match(os_handle_zero(), file))
      {
        // tec: is the row_range inclusive?? may fix the file reading issue
        U64 offset_start = row_range.min * column->size;
        U64 offset_end = (row_range.max + 1) * column->size;
        U64 read_file_size = os_file_read(file, r1u64(offset_start, offset_end), data_ptr);
        *out_size = size;
        
        //log_info("range min: %llu , max: %llu", row_range.min, row_range.max);
        if (offset_end > os_properties_from_file(file).size)
        {
          //log_error("Requested read past EOF: offset_end = %llu, file_size = %llu", offset_end, os_properties_from_file(file).size);
        }
        
        if (read_file_size != size)
        {
          // tec: when reading the end of a large file. the calculated size may be different than the
          // read size. i think its something to do with Windows and how its saving the file. but all the data is
          // there, TODO figure out what is going on
          //log_error("failed to read data for non-string column: %.*s", str8_varg(column->disk_path));
          //log_info("calculated size %llu read size %llu", size, read_file_size);
          //os_file_close(file);
          *out_size = read_file_size;
          //return data_ptr;
        }
        
        os_file_close(file);
        ProfEnd();
        return data_ptr;
      }
      else
      {
        log_error("failed to open disk-backed column: %.*s", str8_varg(column->disk_path));
        *out_size = 0;
        ProfEnd();
        return NULL;
      }
    }
    else
    {
      void* data_ptr = column->data + (row_range.min * column->size);
      *out_size = size;
      ProfEnd();
      return data_ptr;
    }
  }
  ProfEnd();
}

internal GDB_StringDataChunk 
gdb_column_get_string_chunk(Arena* arena, GDB_Column* column, Rng1U64 row_range)
{
  ProfBeginFunction();
  
  GDB_StringDataChunk result = {0};
  
  U64 row_count = row_range.max - row_range.min;
  if (row_count == 0)
  {
    result.data = NULL;
    result.offsets = NULL;
    result.size = 0;
    result.row_count = 0;
    return result;
  }
  
  if (column->is_disk_backed)
  {
    OS_Handle file = column->file;
    if (os_handle_match(os_handle_zero(), file))
    {
      file = os_file_open(OS_AccessFlag_Read, column->disk_path);
    }
    
    U64 variable_reserved = 0;
    os_file_read(file, r1u64(0, sizeof(U64)), &variable_reserved); 
    
    
    U64 start_offset = 0;
    U64 end_offset = 0;
    //U64 offset_position_start = sizeof(U64) + variable_reserved + (row_range.min * sizeof(U64));
    //U64 offset_position_end = sizeof(U64) + variable_reserved + (row_range.max * sizeof(U64));
    U64 offset_position_start = variable_reserved + (row_range.min * sizeof(U64));
    U64 offset_position_end = variable_reserved + (row_range.max * sizeof(U64));
    
    //log_info("off start %llu | off end %llu", offset_position_start, offset_position_end);
    
    if (row_range.min == 0)
    {
      start_offset = 0;
    }
    else
    {
      os_file_read(file, r1u64(offset_position_start - sizeof(U64), offset_position_start), &start_offset);
    }
    
    if (row_range.max == column->row_count)
    {
      end_offset = column->variable_capacity;
      os_file_read(file, r1u64(offset_position_end, offset_position_end + sizeof(U64)), &end_offset);
    }
    else
    {
      os_file_read(file, r1u64(offset_position_end, offset_position_end + sizeof(U64)), &end_offset);
    }
    
    U64 size = end_offset - start_offset;
    result.data = push_array(arena, U8, size);
    if (os_file_read(file, r1u64(start_offset + sizeof(U64), start_offset + size + sizeof(U64)), result.data) != size)
    {
      log_error("Failed to read string data for rows [%llu - %llu]", row_range.min, row_range.max);
      //os_file_close(file);
      //result.data = NULL;
      //result.size = 0;
      //return result;
    }
    
    result.offsets = push_array(arena, U64, row_count);
    if (row_range.min == 0)
    {
      result.offsets[0] = 0;
    }
    else
    {
      U64 prev_offset = 0;
      os_file_read(file, r1u64(offset_position_start - sizeof(U64), offset_position_start), &prev_offset);
      result.offsets[0] = 0;
    }
    
    for (U64 i = 1; i < row_count; i++)
    {
      U64 current_offset = 0;
      os_file_read(file, r1u64(offset_position_start + (i * sizeof(U64)), offset_position_start + ((i + 1) * sizeof(U64))), &current_offset);
      result.offsets[i] = current_offset - start_offset;
    }
    
    if (os_handle_match(os_handle_zero(), column->file))
    {
      os_file_close(file);
    }
    result.size = size;
    result.row_count = row_count;
  }
  else
  {
    U64 start_offset = (row_range.min > 0) ? column->offsets[row_range.min - 1] : 0;
    U64 end_offset = column->offsets[row_range.max - 1];
    U64 size = end_offset - start_offset;
    
    result.data = column->data + start_offset;
    result.size = size;
    result.row_count = row_count;
    
    // tec: NOTE add 1 to the row count to include the last offset
    result.offsets = push_array(arena, U64, row_count+1);
    for (U64 i = 0; i < row_count+1; i++)
    {
      result.offsets[i] = (row_range.min + i > 0) ? column->offsets[row_range.min + i - 1] - start_offset : 0;
    }
  }
  
  ProfEnd();
  return result;
}

internal String8
gdb_generate_disk_path_for_column(Arena* arena, GDB_Column* column)
{
  GDB_Table* table = column->parent_table;
  GDB_Database* database = table->parent_database;
  
  // tec: check for valid paths
  {
    Temp scratch = scratch_begin(0, 0);
    
    String8 database_path = push_str8f(arena, "data/%.*s/", str8_varg(database->name));
    
    if (!os_file_path_exists(database_path))
    {
      os_make_directory(database_path);
    }
    
    String8 table_path = push_str8f(arena, "data/%.*s/%.*s/", str8_varg(database->name), str8_varg(table->name));
    if (!os_file_path_exists(table_path))
    {
      os_make_directory(table_path);
    }
    
    scratch_end(scratch);
  }
  
  String8 column_path = push_str8f(arena, "data/%.*s/%.*s/%.*s.dat", str8_varg(database->name), str8_varg(table->name), str8_varg(column->name));
  return column_path;
}

internal void
gdb_column_convert_to_disk_backed(GDB_Column* column)
{
  ProfBeginFunction();
  
  Temp scratch = scratch_begin(0, 0);
  String8 column_path = gdb_generate_disk_path_for_column(scratch.arena, column);
  OS_Handle file = os_file_open(OS_AccessFlag_Read | OS_AccessFlag_Write | OS_AccessFlag_Append, column_path);
  
  if (column->type == GDB_ColumnType_String8)
  {
    os_file_write(file, r1u64(0, sizeof(U64)), &column->variable_capacity);
    os_file_write(file, r1u64(sizeof(U64), column->variable_capacity), column->data);
    os_file_write(file, r1u64(sizeof(U64) + column->variable_capacity, sizeof(U64) + column->variable_capacity + (column->row_count * sizeof(U64))), column->offsets);
  }
  else
  {
    os_file_write(file, r1u64(0, column->row_count * column->size), column->data);
  }
  
  //os_file_close(file);
  
  column->is_disk_backed = 1;
  column->disk_path = push_str8_copy(column->arena, column_path);
  column->file = file;
  
  column->data = NULL;
  column->offsets = NULL;
  column->capacity = 0;
  
  scratch_end(scratch);
  
  ProfEnd();
}

//~ tec: utils
internal GDB_ColumnType
gdb_column_type_from_string(String8 str)
{
  if (str8_match(str, str8_lit("u32"), StringMatchFlag_CaseInsensitive))
  {
    return GDB_ColumnType_U32;
  }
  else if (str8_match(str, str8_lit("u64"), StringMatchFlag_CaseInsensitive))
  {
    return GDB_ColumnType_U64;
  }
  else if (str8_match(str, str8_lit("f32"), StringMatchFlag_CaseInsensitive))
  {
    return GDB_ColumnType_F32;
  }
  else if (str8_match(str, str8_lit("f64"), StringMatchFlag_CaseInsensitive))
  {
    return GDB_ColumnType_F64;
  }
  else if (str8_match(str, str8_lit("string8"), StringMatchFlag_CaseInsensitive))
  {
    return GDB_ColumnType_String8;
  }
  
  log_error("failed to find matching GDB_ColumnType for '%.*s'", (int)str.size, str.str);
  return GDB_ColumnType_U64;
}

internal String8
string_from_gdb_column_type(GDB_ColumnType type)
{
  String8 result = str8_lit("");
  switch (type)
  {
    case GDB_ColumnType_U32: { result = str8_lit("GDB_ColumnType_U32"); } break;
    case GDB_ColumnType_U64: { result = str8_lit("GDB_ColumnType_U64"); } break;
    case GDB_ColumnType_F32: { result = str8_lit("GDB_ColumnType_F32"); } break;
    case GDB_ColumnType_F64: { result = str8_lit("GDB_ColumnType_F64"); } break;
    case GDB_ColumnType_String8: { result = str8_lit("GDB_ColumnType_String8"); } break;
  }
  return result;
}

internal GDB_ColumnSchema
gdb_column_schema_create(String8 name, GDB_ColumnType type)
{
  GDB_ColumnSchema schema = (GDB_ColumnSchema){ name, type, g_gdb_column_type_size[type] };
  return schema;
}

internal GDB_ColumnType
gdb_infer_column_type(String8 value)
{
  if (!value.str || value.size == 0 || str8_match(value, str8_lit("NULL"), StringMatchFlag_CaseInsensitive))
  {
    //log_error("could not infer column type, string is invalid");
    return GDB_ColumnType_Invalid;
  }
  
  if (str8_is_numeric(value))
  {
    if (!str8_contains(value, '.'))
    {
      U64 u64_value = u64_from_str8(value, 10);
      if (u64_value <= max_U32)
      {
        return GDB_ColumnType_U32;
      }
      
      if (u64_value > max_U32)
      {
        return GDB_ColumnType_U64;
      }
    }
    
    F64 f64_value = f64_from_str8(value);
    if (f64_value)
    {
      return (f64_value == (F32)f64_value) ? GDB_ColumnType_F32 : GDB_ColumnType_F64;
    }
  }
  return GDB_ColumnType_String8;
}

internal GDB_ColumnType
gdb_promote_type(GDB_ColumnType existing, GDB_ColumnType new_type)
{
  if (existing == GDB_ColumnType_Invalid) return new_type;
  if (new_type == GDB_ColumnType_Invalid) return existing;
  
  if (existing == new_type) return existing;
  
  if ((existing == GDB_ColumnType_U32 && new_type == GDB_ColumnType_U64) ||
      (existing == GDB_ColumnType_U64 && new_type == GDB_ColumnType_U32))
  {
    return GDB_ColumnType_U64;
  }
  
  if ((existing == GDB_ColumnType_F32 && new_type == GDB_ColumnType_F64) ||
      (existing == GDB_ColumnType_F64 && new_type == GDB_ColumnType_F32))
  {
    return GDB_ColumnType_F64;
  }
  
  return GDB_ColumnType_String8;
}