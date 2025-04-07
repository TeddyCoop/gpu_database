#define BUILD_ENTRY_DEFINING_UNIT 1

#include "base/base_inc.h"
#include "os/os_inc.h"
#include "gdb/gdb_inc.h"
#include "ir_gen/ir_gen_inc.h"
#include "gpu/gpu_inc.h"
#include "application.h"
#include "thread_pool/thread_pool.h"

internal void test_print_database(GDB_Database *database);

#define ProfCodeBegin(name) \
U64 __prof_start_##name = os_now_microseconds();

#define ProfCodeEnd(name) \
U64 __prof_elapsed_##name = os_now_microseconds() - __prof_start_##name; \
log_info("%s took:: %llu milliseconds", #name, __prof_elapsed_##name / 1000);

#include "base/base_inc.c"
#include "os/os_inc.c"
#include "gpu/gpu_inc.c"
#include "ir_gen/ir_gen_inc.c"
#include "gdb/gdb_inc.c"
#include "application.c"
#include "thread_pool/thread_pool.c"

internal void
test_print_database(GDB_Database *database) 
{
  if (!database)
  {
    fprintf(stderr, "Database is NULL.\n");
    return;
  }
  
  fprintf(stderr, "Database: %.*s\n", (U32)database->name.size, database->name.str);
  fprintf(stderr, "Tables: %llu\n", database->table_count);
  
  for (U64 i = 0; i < database->table_count; ++i)
  {
    GDB_Table *table = database->tables[i];
    if (!table)
    {
      fprintf(stderr, "  Table %llu is NULL.\n", i);
      continue;
    }
    
    fprintf(stderr, "  Table %llu: %.*s\n", i, (U32)table->name.size, table->name.str);
    fprintf(stderr, "    Columns: %llu\n", table->column_count);
    fprintf(stderr, "    Rows: %llu\n", table->row_count);
    
    for (U64 j = 0; j < table->column_count; ++j) 
    {
      GDB_Column *column = table->columns[j];
      fprintf(stderr, "      Column %llu: %.*s (Type: %.*s, Size: %llu, Rows: %llu)\n",
              j,
              (U32)column->name.size, column->name.str,
              str8_varg(string_from_gdb_column_type(column->type)),
              column->size,
              column->row_count);
      
      if (column->type == GDB_ColumnType_String8) 
      {
        fprintf(stderr, "        Data: [");
        for (U64 k = 0; k < column->row_count; ++k) 
        {
          /*
          U64 offset = column->offsets[k];
          U64 start = column->offsets[k];
          U64 end = (k + 1 < column->row_count) ? column->offsets[k + 1] : column->variable_capacity;
          U64 length = end - start;
          String8 str = { .str = column->data + start, .size = length };
          String8 str = {
            .str = column->variable_data + offset,
            .size = (k + 1 < column->row_count) ? column->offsets[k + 1] - offset : column->size - offset
          };
          fprintf("\"%.*s\"", (U32)str.size, str.str);
          */
          //fprintf("\"%s\"", str.str);
          Temp scratch = scratch_begin(0, 0);
          String8 str = gdb_column_get_string(scratch.arena, column, k);
          fprintf(stderr, "\"%.*s\"", str8_varg(str));
          if (k < column->row_count - 1) fprintf(stderr, ", ");
          scratch_end(scratch);
        }
        fprintf(stderr, "]\n");
      }
      else
      {
        fprintf(stderr, "        Data: [");
        for (U64 k = 0; k < column->row_count; ++k) 
        {
          if (column->type == GDB_ColumnType_U32)
          {
            fprintf(stderr, "%u", ((U32 *)column->data)[k]);
          }
          else if (column->type == GDB_ColumnType_U64)
          {
            fprintf(stderr, "%llu", ((U64 *)column->data)[k]);
          } 
          else if (column->type == GDB_ColumnType_F32)
          {
            fprintf(stderr, "%0.2f", ((F32*)column->data)[k]);
          }
          else if (column->type == GDB_ColumnType_F64) 
          {
            fprintf(stderr, "%lf", ((F64*)column->data)[k]);
          }
          if (k < column->row_count - 1) fprintf(stderr, ", ");
        }
        fprintf(stderr, "]\n");
      }
    }
  }
}

internal void
entry_point(void)
{
  ProfCodeBegin(entry_point)
  {
    gdb_init();
    ProfCodeBegin(gpu_init)
    {
      gpu_init();
    }
    ProfCodeEnd(gpu_init);
    
    String8 complex_sql_query = str8_lit(
                                         "CREATE DATABASE retail;"
                                         "CREATE TABLE customers ("
                                         "id U32,"
                                         "name String8,\n"
                                         "age U32,\n"
                                         "email String8,\n"
                                         "balance F64\n"
                                         ");\n"
                                         "\n"
                                         "INSERT INTO customers (id, name, age, email, balance) \n"
                                         "VALUES \n"
                                         "(1, 'Alice', 30, 'aliceinawonderland@example123.com', 1023.50),\n"
                                         "(2, 'Bobdatbuilder', 26, 'bob321@example.com', 24.75),\n"
                                         "(3, 'Charlie', 40, 'charliecoca@example.com', 1500.00),\n"
                                         "(4, 'Danny', 65, 'dannybrown@example.com', 190000.00);\n"
                                         "\n"
                                         "SELECT name, email \n"
                                         //"SELECT email \n"
                                         "FROM customers \n"
                                         "WHERE age >= 25 AND (balance > 500 OR name = 'Bob') \n"
                                         //"WHERE age > 25 AND (balance > 1100) \n"
                                         //"WHERE name = 'Bob' \n"
                                         "ORDER BY balance DESC;\n"
                                         "\n"
                                         "DELETE FROM customers WHERE balance < 100;\n"
                                         "\n"
                                         "ALTER TABLE customers ADD COLUMN vip_status U32;\n"
                                         "\n"
                                         );
    
    String8 use_retail_query = str8_lit(
                                        "USE retail;"
                                        "SELECT name, email \n"
                                        "FROM customers \n"
                                        "WHERE age >= 25 AND (balance > 500 OR name = 'Bob') \n"
                                        );
    
    String8 create_test_database_query = str8_lit(
                                                  "CREATE DATABASE retail;"
                                                  "CREATE TABLE customers ("
                                                  "id U32,"
                                                  "name String8,\n"
                                                  "age U32,\n"
                                                  "email String8,\n"
                                                  "balance F64\n"
                                                  ");\n"
                                                  "\n"
                                                  "INSERT INTO customers (id, name, age, email, balance) \n"
                                                  "VALUES \n"
                                                  "(1, 'Alice', 30, 'aliceinawonderland@example123.com', 1023.50),\n"
                                                  "(2, 'Bobdatbuilder', 26, 'bob321@example.com', 24.75),\n"
                                                  "(3, 'Charlie', 40, 'charliecoca@example.com', 1500.00),\n"
                                                  "(4, 'Danny', 65, 'dannybrown@example.com', 190000.00);\n"
                                                  );
    
    String8 human_ai_query = str8_lit(
                                      "CREATE DATABASE human_to_ai_text;"
                                      "IMPORT INTO text FROM 'human_to_ai_text_dataset.csv';"
                                      );
    String8 pets_query = str8_lit(
                                  "CREATE DATABASE adoptable_pets;"
                                  "IMPORT INTO Adoptable_Pets FROM 'Adoptable_Pets.csv';"
                                  );
    
    String8 real_estate_query = str8_lit(
                                         "CREATE DATABASE real_estate;"
                                         "IMPORT INTO real_estate FROM 'real_estate_big.csv';"
                                         );
    
    //app_execute_query(create_test_database_query);
    //app_execute_query(use_retail_query);
    //app_execute_query(human_ai_query);
    //app_execute_query(pets_query);
    app_execute_query(real_estate_query);
  }
  ProfCodeEnd(entry_point)
}

/*

todo:
[x]- when a file is too large. read piece by piece
 [x]-- this might mean memory mapped files
--- if added, it might be possible to keep active memory usage under 1 GB, maybe in the MB region

- a query returned from the gpu should be a sparse array, only holding the indices of valid elements
-- if a column count is greater than a threshold, split the query up as to not run out of GPU memory
--- also means keeping track of the max gpu memory and the memory used by the database 

- maybe work on other gpu api layers (CUDA, Vulkan)

- add header to .meta files

- more asserts, logging, better profiling
-- maybe a log file
- comments
- general optimization, but cant optimize until you profile

- LOTS of testing
-- compare against other sql databases
--- speed, size of the database, amount of memory used

*/