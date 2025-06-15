#define BUILD_ENTRY_DEFINING_UNIT 1
#define BUILD_CONSOLE_INTERFACE 1
#define PROFILE_CUSTOM 1
#define ARENA_FREE_LIST 1

#define FORCE_KERNEL_COMPILATION 0
#define PRINT_SELECT_OUTPUT 1
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
  
  String8 create_query = str8_lit("CREATE DATABASE testing;"
                                  "IMPORT INTO large_3col FROM 'large_3col.csv';");
  
  String8 select_query = str8_lit("USE testing;"
                                  "SELECT * FROM large_3col "
                                  "WHERE col_0 == 9999999;");
  //"WHERE col_2 == 'val_2_2';");
  //app_execute_query(create_query);
  app_execute_query(select_query);
  
  
  ProfEnd();
  ProfEndCapture();
  log_release();
}