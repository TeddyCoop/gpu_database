#define BUILD_ENTRY_DEFINING_UNIT 1
#define BUILD_CONSOLE_INTERFACE 1
#define PROFILE_CUSTOM 1

#include "base/base_inc.h"
#include "os/os_inc.h"
#include "gdb/gdb_inc.h"
#include "ir_gen/ir_gen_inc.h"
#include "gpu/gpu_inc.h"
#include "application.h"
#include "thread_pool/thread_pool.h"

internal void test_print_database(GDB_Database *database);

#include "base/base_inc.c"
#include "os/os_inc.c"
#include "gpu/gpu_inc.c"
#include "ir_gen/ir_gen_inc.c"
#include "gdb/gdb_inc.c"
#include "application.c"
#include "thread_pool/thread_pool.c"

internal void
entry_point(void)
{
  ProfBeginCapture();
  ProfBeginFunction();
  
  log_alloc();
  
  // tec: create needed folders
  {
    if (!os_file_path_exists(str8_lit("data/")))
    {
      os_make_directory(str8_lit("data/"));
    }
    if (!os_file_path_exists(str8_lit("kernel_cache/")))
    {
      os_make_directory(str8_lit("kernel_cache/"));
    }
  }
  
  gdb_init();
  gpu_init();
  
  log_info("total gpu memory: %llu (MB)", gpu_device_total_memory() >> 20);
  
  String8 use_retail_query = str8_lit(
                                      "USE retail;"
                                      "SELECT name, email \n"
                                      "FROM customers \n"
                                      //"WHERE age >= 25 AND (balance > 500 OR name CONTAINS 'ob') \n"
                                      "WHERE name == 'Theodore';\n"
                                      //"WHERE name contains 'Da';\n"
                                      //"WHERE name contains 'ice';\n"
                                      //"WHERE id == 2;\n"
                                      //"WHERE id > 0;\n"
                                      );
  
  String8 create_test_database_query = str8_lit(
                                                "CREATE DATABASE retail;"
                                                "CREATE TABLE customers ("
                                                "id U64,"
                                                "name String8,\n"
                                                "age U32,\n"
                                                "email String8,\n"
                                                "balance F64\n"
                                                ");\n"
                                                "\n"
                                                "INSERT INTO customers (id, name, age, email, balance) \n"
                                                "VALUES \n"
                                                "(1, 'Alice', 30, 'aliceinawonderland@example123.com', 1023.50),\n"
                                                "(2, 'Bob', 26, 'bob321@example.com', 24.75),\n"
                                                "(3, 'Charlie', 40, 'charliecoca@example.com', 1500.00),\n"
                                                "(4, 'Theodore', 65, 'dannybrown@example.com', 190000.00);\n"
                                                );
  
  String8 test1_create_query = str8_lit(
                                        "CREATE DATABASE test;"
                                        "IMPORT INTO test1 FROM 'test1.csv';"
                                        );
  
  String8 test1_select_query = str8_lit(
                                        "USE test;"
                                        "SELECT col_0_str FROM test1 "
                                        "WHERE col_1_int == 235483;"
                                        );
  
  String8 large_dataset_create_query = str8_lit(
                                                "CREATE DATABASE test2;"
                                                "IMPORT INTO gen_dataset_1_8gb FROM 'gen_dataset_1_8gb.csv';"
                                                );
  
  String8 large_dataset_select_query = str8_lit(
                                                "USE test2;"
                                                "SELECT col_0_str FROM gen_dataset_1_8gb "
                                                //"SELECT col_1_int FROM gen_dataset_1_8gb "
                                                //"WHERE col_0_str == 'R5FsosEspfOgAYWPWkT0pzafrGU5pDr25rB9';"
                                                "WHERE col_1_int == 606126;"
                                                );
  
  //app_execute_query(complex_sql_query);
  //app_execute_query(create_test_database_query);
  //app_execute_query(use_retail_query);
  //app_execute_query(large_dataset_create_query);
  app_execute_query(large_dataset_select_query);
  //app_execute_query(test1_create_query);
  //app_execute_query(test1_select_query);
  //app_execute_query(pets_query);
  //app_execute_query(real_estate_query);
  
  log_release();
  
  ProfEnd();
  ProfEndCapture();
}

/*

todo:
[x]- when a file is too large. read piece by piece
 [x]-- this might mean memory mapped files
[x]--- if added, it might be possible to keep active memory usage under 1 GB, maybe in the MB region

[x]- a query returned from the gpu should be a sparse array, only holding the indices of valid elements
[x]-- if a column count is greater than a threshold, split the query up as to not run out of GPU memory
--- also means keeping track of the max gpu memory and the memory used by the database 

- write a function that parses and adds data to a column

- maybe work on other gpu api layers (CUDA, Vulkan)

- more asserts, logging, better profiling
- comments
- general optimization, but cant optimize until you profile
*/