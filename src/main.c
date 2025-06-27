#define BUILD_ENTRY_DEFINING_UNIT 1
#define BUILD_CONSOLE_INTERFACE 1
#define PROFILE_CUSTOM 1
#define ARENA_FREE_LIST 1

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
entry_point(CmdLine* cmdline)
{
  ProfBeginCapture();
  ProfBeginFunction();
  
  log_alloc();
  
  // tec: create needed folders
  {
    if (!os_file_path_exists(str8_lit("gdb_data/")))
    {
      os_make_directory(str8_lit("gdb_data/"));
    }
    if (!os_file_path_exists(str8_lit("kernel_cache/")))
    {
      os_make_directory(str8_lit("kernel_cache/"));
    }
  }
  
  gdb_init();
  gpu_init();
  
  log_info("total gpu memory: %llu (MB)", gpu_device_total_memory() >> 20);
  
  String8 query_str = cmd_line_string(cmdline, str8_lit("query"));
  B32 valid_query = 0;
  {
    valid_query = query_str.size == 0 ? 0 : 1;
  }
  
  if (valid_query)
  {
    app_execute_query(query_str);
  }
  else
  {
    log_info("invalid query %.*s", str8_varg(query_str));
  }
  
  /*
  String8 create_query = str8_lit("CREATE DATABASE testing;"
                                  "IMPORT INTO xlong_2col FROM 'xlong_2col.csv';");
  
  String8 select_query = str8_lit("USE testing;"
                                  "SELECT * FROM xlong_2col "
                                  "WHERE col_0 == 467;");
  app_execute_query(create_query);
  //app_execute_query(select_query);
  */
  
  /*
  String8 create_query = str8_lit("CREATE DATABASE testing;"
                                  "IMPORT INTO short_2col FROM 'short_2col.csv';");
  
  String8 select_query = str8_lit("USE testing;"
                                  "SELECT * FROM short_2col "
                                  "WHERE col_0 == 0;");
  */
  
  /*
  String8 create_query = str8_lit("CREATE DATABASE testing;"
                                  "IMPORT INTO large_3col FROM 'large_3col.csv';");
  */
  
  //String8 select_query = str8_lit("SELECT * FROM data WHERE (col_8 >= 810272 AND (col_6 >= 826589 OR (236.74 < col_1 < 1122.1) OR (2938.12 < col_2 < 3680.28) OR (3925.79 < col_0 < 4501.07)) AND ((2007.37 < col_1 < 2099.66) OR col_9 >= 626703 OR col_7 >= 908574));");
  //String8 select_query = str8_lit("SELECT * FROM data WHERE (col_8 >= 810272 AND (col_6 >= 826589 OR ((236.74 < col_1) AND (col_1 < 1122.1))));");
  //"WHERE col_2 == 'val_2_2';");
  //app_execute_query(create_query);
  //app_execute_query(select_query);
  
  ProfEnd();
  ProfEndCapture();
  log_release();
}