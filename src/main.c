#define BUILD_ENTRY_DEFINING_UNIT 1
#define BUILD_CONSOLE_INTERFACE 1
#define PROFILE_CUSTOM 1

#define FORCE_KERNEL_COMPILATION 0
#define PRINT_SELECT_OUTPUT 0
#define GPU_MAX_BUFFER_SIZE MB(512)

#include "base/base_inc.h"
#include "os/os_inc.h"
#include "gdb/gdb_inc.h"
#include "ir_gen/ir_gen_inc.h"
#include "gpu/gpu_inc.h"
#include "application.h"
#include "thread_pool/thread_pool.h"

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
  
  /*
  String8 use_retail_query = str8_lit(
                                      "USE retail;"
                                      "SELECT name, email \n"
                                      "FROM customers \n"
                                      //"WHERE age >= 25 AND (balance > 500 OR name CONTAINS 'ob' OR name == 'Bob') \n"
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
  */
  
  String8 medium_create_query = str8_lit(
                                         "CREATE DATABASE medium;"
                                         "IMPORT INTO medium FROM 'medium_25col.csv';"
                                         );
  
  String8 medium_select_query = str8_lit(
                                         "USE medium;"
                                         "SELECT * FROM medium_25col "
                                         "WHERE (col_12 >= 911293 OR col_21 >= 447936 OR col_22 >= 2963.61 AND col_22 <= 3426.63 OR col_14 == 'bGmaA');"
                                         );
  
  String8 large_create_query = str8_lit(
                                        "CREATE DATABASE large;"
                                        "IMPORT INTO large FROM 'large_50col.csv';"
                                        );
  
  
  String8 huge_create_query = str8_lit(
                                       "CREATE DATABASE huge;"
                                       "IMPORT INTO huge FROM 'huge_100col.csv';"
                                       );
  
  String8 huge_select_query = str8_lit(
                                       "USE huge;"
                                       "SELECT * FROM huge_100col "
                                       "WHERE (col_51 == 'B9COt' OR col_82 >= 879.49 AND col_82 <= 1129.8) AND (col_23 >= 581547 OR col_42 == 'W8Xug' OR col_86 >= 573325)"
                                       );
  
  String8 massive_create_query = str8_lit(
                                          "CREATE DATABASE massive;"
                                          "IMPORT INTO massive FROM 'massive_2col.csv';"
                                          );
  
  String8 massive_select_query = str8_lit(
                                          "USE massive;"
                                          "SELECT * FROM massive_2col "
                                          "WHERE (col_1 >= 4070.46 AND col_1 <= 4371.73 OR col_0 >= 197555) AND (col_0 >= 846962 OR col_1 >= 1012.13 AND col_1 <= 1036.33);"
                                          );
  
  String8 triple_string_create_query = str8_lit(
                                                "CREATE DATABASE triple_string;"
                                                "IMPORT INTO triple_string_3col FROM 'triple_string_3col.csv';"
                                                );
  
  String8 triple_string_select_query = str8_lit(
                                                "USE triple_string;"
                                                "SELECT * FROM triple_string_3col "
                                                "WHERE (col_0 == 'LZ2KU' OR col_2 == 'iZaBC');"
                                                );
  
  //app_execute_query(medium_create_query);
  //app_execute_query(medium_select_query);
  //app_execute_query(large_create_query);
  //app_execute_query(massive_create_query);
  //app_execute_query(massive_select_query);
  //app_execute_query(huge_create_query);
  //app_execute_query(huge_select_query);
  //app_execute_query(triple_string_create_query);
  app_execute_query(triple_string_select_query);
  
  ProfEnd();
  ProfEndCapture();
  log_release();
}