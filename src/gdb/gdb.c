internal void
gdb_init(void)
{
  Arena* arena = arena_alloc(.reserve_size=GB(2), .commit_size=MB(32));
  g_gdb_state = push_array(arena, GDB_State, 1);
  g_gdb_state->arena = arena;
  
  g_gdb_state->databases = NULL;
}

internal void
gdb_release(void)
{
  arena_release(g_gdb_state->arena);
}

internal void
gdb_add_database(GDB_Database* database)
{
  if (g_gdb_state->database_count == 0)
  {
    g_gdb_state->databases = push_array(g_gdb_state->arena, GDB_Database*, 4);
    g_gdb_state->database_capacity = 4;
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
}

//~ tec: database
internal GDB_Database*
gdb_database_alloc(String8 name)
{
  Arena* arena = arena_alloc(.reserve_size=KB(64), .commit_size=KB(4));
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
    database->tables = push_array(database->arena, GDB_Table*, 8);
    database->table_capacity = 8;
  }
  else if (database->table_count >= database->table_capacity)
  {
    U64 new_capacity = database->table_capacity * 2;
    GDB_Table** new_tables = push_array(database->arena, GDB_Table*, new_capacity);
    MemoryCopy(new_tables, database->tables, sizeof(GDB_Table*) * database->table_count);
    database->tables = new_tables;
    database->table_capacity = new_capacity;
  }
  
  database->tables[database->table_count++] = table;
}

global String8 g_gdb_database_save_path = str8_lit_comp("data/");

internal B32
gdb_database_save(GDB_Database* database, String8 directory)
{
  Temp scratch = scratch_begin(0, 0);
  
  // tec: check if the last character is a slash
  {
    U8 last_char = directory.str[directory.size-1];
    if (last_char == '/' || last_char == '\\')
    {
    }
    else
    {
      directory = push_str8_cat(scratch.arena, directory, str8_lit("/"));
    }
  }
  
  if (!os_make_directory(directory))
  {
    log_error("failed to create/open database directory: %s", directory.str);
    return 0;
  }
  
  for (U64 i = 0; i < database->table_count; i++)
  {
    GDB_Table* table = database->tables[i];
    String8 file_path = push_str8f(scratch.arena, "%s%.*s.gdbt", directory.str, (U32)table->name.size, table->name.str);
    if (!gdb_table_save(table, file_path))
    {
      log_error("failed to save table: %s", table->name.str);
      return 0;
    }
  }
  scratch_end(scratch);
  
  return 1;
}

internal GDB_Database*
gdb_database_load(String8 directory_path)
{
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
  
  OS_FileIter* it = os_file_iter_begin(scratch.arena, directory_path, 0);
  U64 idx = 0;
  for(OS_FileInfo info = {0}; idx < 16384 && os_file_iter_next(scratch.arena, it, &info); idx += 1)
  {
    if (str8_ends_with(info.name, str8_lit(".gdbt"), 0))
    {
      String8 file_path = push_str8_cat(scratch.arena, directory_path, info.name);
      GDB_Table* table = gdb_table_load(file_path);
      if (table)
      {
        gdb_database_add_table(database, table);
      }
      else
      {
        log_error("failed to load table: %s", file_path.size);
      }
    }
  }
  
  scratch_end(scratch);
  return database;
}

internal void
gdb_database_close(GDB_Database* database)
{
  
}

internal GDB_Table* 
gdb_database_find_table(GDB_Database* database, String8 table_name)
{
  for (U64 i = 0; i < database->table_count; i++)
  {
    GDB_Table* table = database->tables[i];
    if (str8_match(table->name, table_name, 0))
    {
      return table;
    }
  }
  log_error("failed to find table '%.*s' in database '%.*s", table_name.size, table_name.str,
            database->name.size, database->name.str);
  return NULL;
}

//~ tec: tables


internal GDB_Table*
gdb_table_alloc(String8 name)
{
  Arena* arena = arena_alloc(.reserve_size=MB(64), .commit_size=KB(64));
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
  if (table->column_count == 0)
  {
    table->columns = push_array(table->arena, GDB_Column*, 8);
    table->column_capacity = 8;
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
  table->columns[table->column_count++] = column;
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
gdb_table_save(GDB_Table* table, String8 path)
{
  OS_Handle file = os_file_open(OS_AccessFlag_Read | OS_AccessFlag_Write, path);
  if (os_handle_match(os_handle_zero(), file))
  {
    log_error("failed to open table file: %s", path.str);
    return 0;
  }
  
  U64 file_size = sizeof(U64) * 2; // column_count + row_count
  
  for (U64 i = 0; i < table->column_count; i++)
  {
    GDB_Column* column = table->columns[i];
    
    // tec: meta size
    file_size += sizeof(GDB_ColumnType) + sizeof(U64) * 2 + column->name.size;
    
    // tec: data size
    if (column->type == GDB_ColumnType_String8)
    {
      file_size += column->variable_capacity + column->capacity * sizeof(U64);
    }
    else
    {
      file_size += column->capacity * column->size;
    }
  }
  
  Temp scratch = scratch_begin(0, 0);
  U8* write_ptr = push_array(scratch.arena, U8, file_size);
  U8* buffer_start = write_ptr;
  
  // tec: write table metadata
  *(U64*)write_ptr = table->column_count; write_ptr += sizeof(U64);
  *(U64*)write_ptr = table->row_count; write_ptr += sizeof(U64);
  
  // tec: write columns
  for (U64 i = 0; i < table->column_count; i++)
  {
    GDB_Column* column = table->columns[i];
    
    // tec: write column metadata
    *(GDB_ColumnType*)write_ptr = column->type; write_ptr += sizeof(GDB_ColumnType);
    *(U64*)write_ptr = column->size; write_ptr += sizeof(U64);
    *(U64*)write_ptr = column->capacity; write_ptr += sizeof(U64);
    *(U64*)write_ptr = column->name.size; write_ptr += sizeof(U64);
    MemoryCopy(write_ptr, column->name.str, column->name.size);
    write_ptr += column->name.size;
    
    // tec: write column data
    if (column->type == GDB_ColumnType_String8)
    {
      *(U64*)write_ptr = column->variable_capacity; write_ptr += sizeof(U64);
      MemoryCopy(write_ptr, column->data, column->variable_capacity);
      write_ptr += column->variable_capacity;
      MemoryCopy(write_ptr, column->offsets, column->capacity * sizeof(U64));
      write_ptr += column->capacity * sizeof(U64);
    }
    else
    {
      MemoryCopy(write_ptr, column->data, column->capacity * column->size);
      write_ptr += column->capacity * column->size;
    }
  }
  
  os_file_write(file, r1u64(0, file_size), buffer_start);
  os_file_close(file);
  
  scratch_end(scratch);
  
  return 1;
}

internal B32
gdb_table_export_csv(GDB_Table* table, String8 path)
{
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
        output = gdb_column_get_string(column, row);
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
  
  return 1;
}

internal GDB_Table*
gdb_table_load(String8 path)
{
  GDB_Table* table = gdb_table_alloc(str8_lit("temp"));
  
  Temp scratch = temp_begin(g_gdb_state->arena);
  String8 file_data = os_data_from_file_path(scratch.arena, path);
  U8* read_ptr = file_data.str;
  
  // tec: read metadata
  table->column_count = *(U64*)read_ptr; read_ptr += sizeof(U64);
  table->row_count = *(U64*)read_ptr; read_ptr += sizeof(U64);
  
  // tec: read columns
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
    
    if (column->type == GDB_ColumnType_String8)
    {
      column->variable_capacity = *(U64*)read_ptr; read_ptr += sizeof(U64);
      
      column->data = push_array(table->arena, U8, column->variable_capacity);
      MemoryCopy(column->data, read_ptr, column->variable_capacity);
      read_ptr += column->variable_capacity;
      
      column->offsets = push_array(table->arena, U64, column->capacity);
      MemoryCopy(column->offsets, read_ptr, column->capacity * sizeof(U64));
      read_ptr += column->capacity * sizeof(U64);
    }
    else
    {
      column->data = read_ptr;
      read_ptr += column->capacity * column->size;
    }
    
    table->columns[i] = column;
  }
  
  table->name = str8_chop_last_dot(str8_skip_last_slash(path));
  
  temp_end(scratch);
  
  return table;
}

internal GDB_Table*
gdb_table_import_csv(String8 path)
{
  GDB_Table* table = gdb_table_alloc(str8_lit("temp"));
  
  Temp scratch = temp_begin(g_gdb_state->arena);
  String8 file_data = os_data_from_file_path(scratch.arena, path);
  
  String8List rows = str8_split_by_string_chars(scratch.arena, file_data, str8_lit("\n"), 0);
  
  if (rows.node_count == 0) return table;
  
  String8List columns = str8_split_by_string_chars(scratch.arena, rows.first->string, str8_lit(","), StringSplitFlag_RespectQuotes);
  U64 column_count = columns.node_count;
  
  if (column_count == 0)
  {
    log_error("no columns found in csv: %s", path.str);
    return NULL;
  }
  
  GDB_ColumnType* column_types = push_array(scratch.arena, GDB_ColumnType, column_count);
  MemorySet(column_types, GDB_ColumnType_Invalid, column_count * sizeof(GDB_ColumnType));
  
  //- tec: infer column types
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
  
  //- tec: create columns
  U64 col_index = 0;
  for (String8Node* col = columns.first; col; col = col->next, col_index++) 
  {
    String8 str = push_str8_copy(table->arena, col->string);
    if (column_types[col_index] == GDB_ColumnType_Invalid)
    {
      column_types[col_index] = GDB_ColumnType_String8;
    }
    GDB_ColumnSchema schema = gdb_column_schema_create(str, column_types[col_index]);
    gdb_table_add_column(table, schema);
  }
  
  //- tec: import csv data into columns
  row_index = 0;
  for (String8Node* row = rows.first->next; row; row = row->next, row_index++)
  {
    String8List values = str8_split_by_string_chars(scratch.arena, row->string, str8_lit(","), StringSplitFlag_RespectQuotes | StringSplitFlag_KeepEmpties);
    
    col_index = 0;
    for (String8Node* val = values.first; val && col_index < column_count; val = val->next, col_index++) 
    {
      GDB_Column* column = table->columns[col_index];
      
      if (val->string.size == 0)
      {
        if (column->type == GDB_ColumnType_String8)
        {
          String8 empty_str = {0};
          gdb_column_add_data(column, &empty_str);
        }
        else if (column->type == GDB_ColumnType_F64)
        {
          F64 empty_f64 = 0.0;
          gdb_column_add_data(column, &empty_f64);
        }
        else if (column->type == GDB_ColumnType_U64)
        {
          U64 empty_u64 = 0;
          gdb_column_add_data(column, &empty_u64);
        }
      }
      else
      {
        if (column->type == GDB_ColumnType_String8)
        {
          gdb_column_add_data(column, &val->string);
        }
        else if (column->type == GDB_ColumnType_F64)
        {
          F64 data = f64_from_str8(val->string);
          gdb_column_add_data(column, &data);
        }
        else if (column->type == GDB_ColumnType_U64)
        {
          U64 data = u64_from_str8(val->string, 10);
          gdb_column_add_data(column, &data);
        }
      }
    }
  }
  
  table->name = str8_chop_last_dot(str8_skip_last_slash(path));
  table->row_count = row_index;
  
  temp_end(scratch);
  
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
  Arena* arena = arena_alloc(.reserve_size=GB(1), .commit_size=MB(32));
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
gdb_column_add_data(GDB_Column* column, void* data)
{
  if (column->type == GDB_ColumnType_String8)
  {
    String8* str = (String8*)data;
    
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
      U64 new_variable_capacity = (column->variable_capacity > 0) ? column->variable_capacity * 2 : KB(4);
      while (new_variable_capacity < required_size)
      {
        new_variable_capacity *= 2;
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
      //return;
    }
    
    U64 current_offset = (column->row_count > 0) ? column->offsets[column->row_count - 1] : 0;
    MemoryCopy(column->data + current_offset, str->str, str->size);
    column->offsets[column->row_count] = current_offset + str->size;
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
      if (new_capacity > column->capacity + MB(64))
      {
        new_capacity = column->capacity + MB(64);
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
  return (void*)(column->data + index * column->size);
}

internal String8
gdb_column_get_string(GDB_Column* column, U64 index)
{
  String8 result = { 0 };
  
  if (index >= column->row_count || !column->offsets)
  {
    return result;
  }
  
  if (column->type == GDB_ColumnType_String8)
  {
    U64 start = (index > 0) ? column->offsets[index - 1] : 0;
    U64 end = column->offsets[index];
    
    if (start < end && end <= column->variable_capacity)
    {
      result.str = column->data + start;
      result.size = end - start;
    }
  }
  
  return result;
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
  if (value.size == 0 || str8_match(value, str8_lit("NULL"), StringMatchFlag_CaseInsensitive))
  {
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
