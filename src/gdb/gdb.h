/* date = January 25th 2025 11:47 am */

#ifndef GDB_H
#define GDB_H

#define GDB_FILE_FORMAT_VERSION_MAJOR 0
#define GDB_FILE_FORMAT_VERSION_MINOR 1

/*
file format:

[database header]
name - String8
table count - U64

  this will be an array of tables
[table header]
name - String8
column count - U64
 row count - U64

array of columns
[column header]
name - String8
type - U8 (enum)
capacity - U64
data - U8*
if variable width data
offsets - U64*

*/

#define GDB_COLUMN_EXPAND_COUNT 64
#define GDB_TABLE_EXPAND_FACTOR 2

typedef U32 GDB_ColumnType;
enum
{
  GDB_ColumnType_Invalid,
  GDB_ColumnType_U32,
  GDB_ColumnType_U64,
  GDB_ColumnType_F32,
  GDB_ColumnType_F64,
  GDB_ColumnType_String8,
  GDB_ColumnType_COUNT
};

global U64 g_gdb_column_type_size[GDB_ColumnType_COUNT] = 
{ 
  0,
  sizeof(U32),
  sizeof(U64),
  sizeof(F32),
  sizeof(F64),
  sizeof(String8),
};

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
internal B32 gdb_database_save(GDB_Database* database, String8 directory);
internal GDB_Database* gdb_database_load(String8 directory);
internal void gdb_database_close(GDB_Database* database);
internal GDB_Table* gdb_database_find_table(GDB_Database* database, String8 table_name);

internal GDB_Table* gdb_table_alloc(String8 name);
internal void gdb_table_release(GDB_Table* table);
internal void gdb_table_add_column(GDB_Table* table, GDB_ColumnSchema schema);
internal void gdb_table_add_row(GDB_Table* table, void** row_data);
internal void gdb_table_remove_row(GDB_Table* table, U64 row_index);
internal B32 gdb_table_save(GDB_Table* table, String8 path);
internal GDB_Table* gdb_table_load(String8 path);
internal GDB_Column* gdb_table_find_column(GDB_Table* table, String8 column_name);

internal GDB_Column* gdb_column_alloc(String8 name, GDB_ColumnType type, U64 size);
internal void gdb_column_release(GDB_Column* column);
internal void gdb_column_add_data(GDB_Column* column, void* data);
internal void gdb_column_remove_data(GDB_Column* column, U64 row_index);
internal void* gdb_column_get_data(GDB_Column* column, U64 index);

internal GDB_ColumnType gdb_column_type_from_string(String8 str);
internal GDB_ColumnSchema gdb_column_schema_create(String8 name, GDB_ColumnType type);

#endif //GDB_H
