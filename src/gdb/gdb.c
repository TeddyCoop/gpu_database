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
    String8 file_path = push_str8f(scratch.arena, "%s%s.gdbt", directory.str, table->name.str);
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
  
  database->name = push_str8_copy(database->arena, str8_chop_last_slash(directory_path));
  
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
    U64 new_capacity = table->column_capacity * 2;
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

internal B32
gdb_table_save(GDB_Table* table, String8 path)
{
  OS_Handle file = os_file_open(OS_AccessFlag_Read | OS_AccessFlag_Write, path);
  if (!file.u64[0])
  {
    log_error("failed to open table file: %s", path.str);
    return 0;
  }
  
  OS_Handle map = os_file_map_open(OS_AccessFlag_Read | OS_AccessFlag_Write, file);
  if (!map.u64[0])
  {
    log_error("failed to create file mapping for: %s", path.str);
    os_file_close(file);
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
  
  // tec: resize and memory map
  void* mapped_ptr = 0;
  if (!os_file_map_resize(&map, file, &mapped_ptr, file_size))
  {
    log_error("failed to memory-map table file: %s", path.str);
    return 0;
  }
  
  U8* write_ptr = (U8*)mapped_ptr;
  
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
      MemoryCopy(write_ptr, column->variable_data, column->variable_capacity);
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
  
  os_file_map_view_close(map, mapped_ptr, (Rng1U64){0, file_size});
  os_file_map_close(map);
  os_file_close(file);
  
  return 1;
}

internal GDB_Table*
gdb_table_load(String8 path)
{
  GDB_Table* table = gdb_table_alloc(str8_lit("temp"));
  
  OS_Handle file = os_file_open(OS_AccessFlag_Read|OS_AccessFlag_ShareRead, path);
  if (os_handle_match(file, os_handle_zero()))
  {
    log_error("Failed to open table file: %s", path.str);
    return NULL;
  }
  
  OS_Handle map = os_file_map_open(OS_AccessFlag_Read, file);
  if (os_handle_match(map, os_handle_zero()))
  {
    log_error("Failed to open file mapping for: %s", path.str);
    os_file_close(file);
    return NULL;
  }
  
  U64 file_size = os_properties_from_file(file).size;
  if (file_size == 0)
  {
    log_error("failed to get file size: %s", path.str);
    os_file_map_close(map);
    os_file_close(file);
    return NULL;
  }
  
  // tec: map file into memory
  void* mapped_ptr = os_file_map_view_open(map, OS_AccessFlag_Read, r1u64(0, file_size));
  if (!mapped_ptr)
  {
    log_error("Failed to map table file: %s", path.str);
    os_file_map_close(map);
    os_file_close(file);
    return NULL;
  }
  
  U8* read_ptr = (U8*)mapped_ptr;
  
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
    column->name.str = read_ptr;
    read_ptr += column->name.size;
    
    if (column->type == GDB_ColumnType_String8)
    {
      column->variable_capacity = *(U64*)read_ptr; read_ptr += sizeof(U64);
      column->variable_data = read_ptr;
      read_ptr += column->variable_capacity;
      column->offsets = (U64*)read_ptr;
      read_ptr += column->capacity * sizeof(U64);
    }
    else
    {
      column->data = read_ptr;
      read_ptr += column->capacity * column->size;
    }
    
    table->columns[i] = column;
  }
  
  table->map = map;
  table->file = file;
  table->mapped_ptr = mapped_ptr;
  
  return table;
}

//~ tec: column
internal GDB_Column*
gdb_column_alloc(String8 name, GDB_ColumnType type, U64 size)
{
  Arena* arena = arena_alloc(.reserve_size=GB(2), .commit_size=MB(32));
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
        MemoryCopy(new_offsets, column->offsets, sizeof(U64) * column->row_count);
      }
      column->offsets = new_offsets;
      column->capacity = new_capacity;
    }
    
    //- tec: grow variable data if needed
    U64 required_size = (column->row_count > 0 ? column->offsets[column->row_count - 1] : 0) + str->size;
    if (required_size > column->variable_capacity)
    {
      U64 new_variable_capacity = (column->variable_capacity > 0) ? column->variable_capacity * 2 : KB(4);
      while (new_variable_capacity < required_size)
      {
        new_variable_capacity *= 2;
      }
      U8* new_variable_data = push_array(column->arena, U8, new_variable_capacity);
      if (column->variable_data) 
      {
        MemoryCopy(new_variable_data, column->variable_data, column->variable_capacity);
      }
      column->variable_data = new_variable_data;
      column->variable_capacity = new_variable_capacity;
    }
    
    // tec: add string data
    //column->offsets[column->row_count] = (column->row_count > 0 ? column->offsets[column->row_count - 1] : 0);
    column->offsets[column->row_count] = (column->row_count > 0 ? column->offsets[column->row_count - 1] : 0) + str->size;
    MemoryCopy(column->variable_data + column->offsets[column->row_count], str->str, str->size);
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