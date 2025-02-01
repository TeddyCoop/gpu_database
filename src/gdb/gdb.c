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
    // Allocate initial memory for the table pointers
    database->tables = push_array(database->arena, GDB_Table*, 8); // Start with space for 8 tables
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
  
  // Add the table
  database->tables[database->table_count++] = table;
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
    // Allocate initial memory for columns
    table->columns = push_array(table->arena, GDB_Column*, 8); // Start with space for 8 columns
    table->column_capacity = 8;
  }
  else if (table->column_count >= table->column_capacity)
  {
    // Grow the columns array if capacity is exceeded
    U64 double_capacity = table->column_capacity * 2;
    U64 new_capacity = double_capacity > Million(1) ? table->column_capacity + (table->column_capacity * 0.5) : double_capacity;
    GDB_Column** new_columns = push_array(table->arena, GDB_Column*, new_capacity);
    MemoryCopy(new_columns, table->columns, sizeof(GDB_Column*) * table->column_count);
    table->columns = new_columns;
    table->column_capacity = new_capacity;
  }
  
  // Allocate and add the new column
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
    
    // Grow offsets array if needed
    if (column->row_count == column->capacity)
    {
      U64 new_capacity = (column->capacity > 0) ? column->capacity * 2 : GDB_COLUMN_EXPAND_COUNT;
      U64* new_offsets = push_array(column->arena, U64, new_capacity);
      if (column->offsets) {
        MemoryCopy(new_offsets, column->offsets, sizeof(U64) * column->row_count);
      }
      column->offsets = new_offsets;
      column->capacity = new_capacity;
    }
    
    // Grow variable data if needed
    U64 required_size = (column->row_count > 0 ? column->offsets[column->row_count - 1] : 0) + str->size;
    if (required_size > column->variable_capacity)
    {
      U64 new_variable_capacity = (column->variable_capacity > 0) ? column->variable_capacity * 2 : KB(4);
      while (new_variable_capacity < required_size) {
        new_variable_capacity *= 2;
      }
      U8* new_variable_data = push_array(column->arena, U8, new_variable_capacity);
      if (column->variable_data) {
        MemoryCopy(new_variable_data, column->variable_data, column->variable_capacity);
      }
      column->variable_data = new_variable_data;
      column->variable_capacity = new_variable_capacity;
    }
    
    // Add the string data
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
      
      //U8* new_data = push_array(column->arena, U8, new_capacity * column->size);
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