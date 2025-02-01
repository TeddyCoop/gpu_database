/* date = January 25th 2025 11:47 am */

#ifndef GDB_H
#define GDB_H

#define GDB_COLUMN_EXPAND_COUNT 128

typedef enum GDB_ColumnType
{
  GDB_ColumnType_U32,
  GDB_ColumnType_U64,
  GDB_ColumnType_F32,
  GDB_ColumnType_F64,
  GDB_ColumnType_String8,
  GDB_ColumnType_COUNT
} GDB_ColumnType;

typedef struct GDB_ColumnSchema GDB_ColumnSchema;
struct GDB_ColumnSchema
{
  String8 name;
  GDB_ColumnType type;
  U64 size;
};

typedef struct GDB_Column GDB_Column;
struct GDB_Column
{
  Arena* arena;
  
  String8 name;
  GDB_ColumnType type;
  U64 size;
  U64 capacity;
  U64 variable_capacity;
  U64 row_count; 
  
  // tec: data storage
  U8 *data;
  U8 *variable_data;
  U64 *offsets;
};

typedef struct GDB_Table GDB_Table;
struct GDB_Table
{
  Arena* arena;
  
  String8 name;
  U64 column_count;
  U64 column_capacity;
  U64 row_count;
  GDB_Column** columns;
};

typedef struct GDB_Database GDB_Database;
struct GDB_Database
{
  Arena* arena;
  
  String8 name;
  U64 table_count;
  U64 table_capacity;
  GDB_Table** tables;
};

typedef struct GDB_State GDB_State;
struct GDB_State
{
  Arena* arena;
  
  GDB_Database** databases;
  U64 database_count;
  U64 database_capacity;
};

global GDB_State* g_gdb_state = 0;

internal void gdb_init(void);
internal void gdb_add_database(GDB_Database* database);

internal GDB_Database* gdb_database_alloc(String8 name);
internal void gdb_database_release(GDB_Database* database);
internal void gdb_database_add_table(GDB_Database* database, GDB_Table* table);

internal GDB_Table* gdb_table_alloc(String8 name);
internal void gdb_table_release(GDB_Table* table);
internal void gdb_table_add_column(GDB_Table* table, GDB_ColumnSchema schema);
internal void gdb_table_add_row(GDB_Table* table, void** row_data);

internal GDB_Column* gdb_column_alloc(String8 name, GDB_ColumnType type, U64 size);
internal void gdb_column_release(GDB_Column* column);
internal void gdb_column_add_data(GDB_Column* column, void* data);

#endif //GDB_H
