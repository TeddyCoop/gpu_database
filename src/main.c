#define BUILD_ENTRY_DEFINING_UNIT 1

#include "base/base_inc.h"
#include "os/os_inc.h"
#include "gdb/gdb_inc.h"
#include "gpu/gpu_inc.h"

#include "base/base_inc.c"
#include "os/os_inc.c"
#include "gpu/gpu_inc.c"
#include "gdb/gdb_inc.c"

#define ProfCodeBegin(name) \
U64 __prof_start_##name = os_now_microseconds();

#define ProfCodeEnd(name) \
U64 __prof_elapsed_##name = os_now_microseconds() - __prof_start_##name; \
log_info("%s took:: %llu microseconds", #name, __prof_elapsed_##name);

internal void
test_print_database(GDB_Database *database) 
{
  if (!database)
  {
    printf("Database is NULL.\n");
    return;
  }
  
  printf("Database: %.*s\n", database->name.size, database->name.str);
  printf("Tables: %llu\n", database->table_count);
  
  for (U64 i = 0; i < database->table_count; ++i)
  {
    GDB_Table *table = database->tables[i];
    if (!table)
    {
      printf("  Table %llu is NULL.\n", i);
      continue;
    }
    
    printf("  Table %llu: %.*s\n", i, table->name.size, table->name.str);
    printf("    Columns: %llu\n", table->column_count);
    printf("    Rows: %llu\n", table->row_count);
    
    for (U64 j = 0; j < table->column_count; ++j) 
    {
      GDB_Column *column = table->columns[j];
      printf("      Column %llu: %.*s (Type: %d, Size: %llu, Rows: %llu)\n",
             j,
             column->name.size, column->name.str,
             column->type,
             column->size,
             column->row_count);
      
      if (column->type == GDB_ColumnType_String8) 
      {
        printf("        Data: [");
        for (U64 k = 0; k < column->row_count; ++k) 
        {
          U64 offset = column->offsets[k];
          U64 start = column->offsets[k];
          U64 end = (k + 1 < column->row_count) ? column->offsets[k + 1] : column->variable_capacity;
          U64 length = end - start;
          String8 str = { .str = column->variable_data + start, .size = length };
          /*
          String8 str = {
            .str = column->variable_data + offset,
            .size = (k + 1 < column->row_count) ? column->offsets[k + 1] - offset : column->size - offset
          };
          */
          printf("\"%.*s\"", str.size, str.str);
          //printf("\"%s\"", str.str);
          if (k < column->row_count - 1) printf(", ");
        }
        printf("]\n");
      }
      else
      {
        printf("        Data: [");
        for (U64 k = 0; k < column->row_count; ++k) 
        {
          if (column->type == GDB_ColumnType_U32)
          {
            printf("%u", ((U32 *)column->data)[k]);
          } else if (column->type == GDB_ColumnType_U64)
          {
            printf("%llu", ((U64 *)column->data)[k]);
          } else if (column->type == GDB_ColumnType_F32)
          {
            printf("%0.2f", ((F32*)column->data)[k]);
          } else if (column->type == GDB_ColumnType_F64) 
          {
            printf("%lf", ((F64*)column->data)[k]);
          }
          if (k < column->row_count - 1) printf(", ");
        }
        printf("]\n");
      }
    }
  }
}

internal void
test_one_million_rows_filter(void)
{
  GDB_Database* database = gdb_database_alloc(str8_lit("database"));
  gdb_add_database(database);
  
  GDB_ColumnSchema schemas[] = 
  {
    { str8_lit("id"), GDB_ColumnType_U64, sizeof(U64) },
  };
  
  GDB_Table* table = gdb_table_alloc(str8_lit("table"));
  gdb_table_add_column(table, schemas[0]);
  gdb_database_add_table(database, table);
  
  srand(42);
  const U64 row_count = Million(100);
  //const U64 row_count = 10;
  log_info("column size %llu", (sizeof(U64) * row_count) / 1024);
  
  for (U64 i = 0; i < row_count; i++)
  {
    U64 value = (U64)(rand() % 100 + 1);
    void* row_data[] = { &value };
    gdb_table_add_row(table, row_data);
  }
  
  printf("Original Table Data:\n");
  for (U64 i = 0; i < row_count; i++)
  {
    //printf("%u ", ((U32*)table->columns[0]->data)[i]);
  }
  printf("\n");
  
  GPU_Table* gpu_table = gpu_table_transfer(table);
  GPU_Kernel* kernel = gpu_kernel_alloc(str8_lit("filter_kernel"), g_filter_kernel_string);
  
  GPU_Buffer* result_buffer = gpu_buffer_alloc(row_count * sizeof(U64), GPU_BufferFlag_ReadWrite);
  GPU_Buffer* result_count = gpu_buffer_alloc(sizeof(U64), GPU_BufferFlag_ReadWrite);
  
  U64 threshold = 50;
  ProfCodeBegin(execute_filter_kernel);
  {
    execute_filter_kernel(kernel, gpu_table, threshold, result_buffer, result_count);
  }
  ProfCodeEnd(execute_filter_kernel);
  
  U64* filtered_results = push_array(g_gdb_state->arena, U64, row_count);
  gpu_buffer_read(result_buffer, filtered_results, row_count * sizeof(U64));
  
  U64 filtered_count = 0;
  gpu_buffer_read(result_count, &filtered_count, sizeof(U64));
  
  // Step 8: Print Filtered Results
  printf("Filtered Values (>= %llu):\n", threshold);
  for (U64 i = 0; i < filtered_count; i++)
  {
    //printf("%llu ", filtered_results[i]);
  }
  printf("\n");
}

internal void
entry_point(void)
{
  ProfCodeBegin(entry_point)
  {
    ProfCodeBegin(gpu_init)
    {
      //gpu_init();
    }
    ProfCodeEnd(gpu_init);
    gdb_init();
    
    GDB_Database* database = gdb_database_alloc(str8_lit("database"));
    gdb_add_database(database);
    
    GDB_ColumnSchema schemas[] = 
    {
      { str8_lit("name"), GDB_ColumnType_String8, 0 },
    };
    
    GDB_Table* table = gdb_table_alloc(str8_lit("table"));
    gdb_table_add_column(table, schemas[0]);
    gdb_database_add_table(database, table);
    
    const U64 row_count = 10;
    
    for (U64 i = 0; i < row_count; i++)
    {
      String8 value = push_str8f(g_gdb_state->arena, "name%llu", i);
      void* row_data[] = { &value };
      gdb_table_add_row(table, row_data);
    }
    
    print_database(database);
    
    /*
    {
    GDB_Database* database = gdb_database_alloc(str8_lit("database"));
    gdb_add_database(database);
    
    GDB_ColumnSchema schemas[] = 
    {
      { str8_lit("id"), GDB_ColumnType_U32, sizeof(U32) },
      { str8_lit("price"), GDB_ColumnType_F32, sizeof(F32) }
    };
    
    GDB_Table* table = gdb_table_alloc(str8_lit("table"));
    gdb_table_add_column(table, schemas[0]);
    gdb_table_add_column(table, schemas[1]);
    gdb_database_add_table(database, table);
    
    for (U32 i = 0; i < 10; i++)
    {
      U32 id = i;
      F32 price = 5.0f * i;
      void* row1[] = { &id, &price };
      gdb_table_add_row(table, row1);
    }
    
    //print_database(database);
    }
      */
  }
  ProfCodeEnd(entry_point)
}
