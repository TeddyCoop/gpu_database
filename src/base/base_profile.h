/* date = July 26th 2024 1:21 pm */

#ifndef BASE_PROFILE_H
#define BASE_PROFILE_H

////////////////////////////////
//~ tec: Zero Settings

#if !defined(PROFILE_TELEMETRY)
# define PROFILE_TELEMETRY 0
#endif

#if !defined(PROFILE_REMOTERY)
# define PROFILE_REMOTERY 0
#endif

#if !defined(MARKUP_LAYER_COLOR)
# define MARKUP_LAYER_COLOR 1.00f, 0.00f, 1.00f
#endif

////////////////////////////////
//~ tec: Third Party Includes

#if PROFILE_TELEMETRY
# include "rad_tm.h"
# if OS_WINDOWS
#  pragma comment(lib, "rad_tm_win64.lib")
# endif
#endif

#if PROFILE_REMOTERY
# include "third_party/Remotery/Remotery.h"
//# include "third_party/Remotery/Remotery.c"
#endif

////////////////////////////////
//~ tec: Telemetry Profile Defines

#if PROFILE_TELEMETRY
# define ProfBegin(...)            tmEnter(0, 0, __VA_ARGS__)
# define ProfBeginDynamic(...)     (TM_API_PTR ? TM_API_PTR->_tmEnterZoneV_Core(0, 0, __FILE__, &g_telemetry_filename_id, __LINE__, __VA_ARGS__) : (void)0)
# define ProfEnd(...)              (TM_API_PTR ? TM_API_PTR->_tmLeaveZone(0) : (void)0)
# define ProfTick(...)             tmTick(0)
# define ProfIsCapturing(...)      tmRunning()
# define ProfBeginCapture(...)     tmOpen(0, __VA_ARGS__, __DATE__, "localhost", TMCT_TCP, TELEMETRY_DEFAULT_PORT, TMOF_INIT_NETWORKING|TMOF_CAPTURE_CONTEXT_SWITCHES, 100)
# define ProfEndCapture(...)       tmClose(0)
# define ProfThreadName(...)       (TM_API_PTR ? TM_API_PTR->_tmThreadName(0, 0, __VA_ARGS__) : (void)0)
# define ProfMsg(...)              (TM_API_PTR ? TM_API_PTR->_tmMessageV_Core(0, TMMF_ICON_NOTE, __FILE__, &g_telemetry_filename_id, __LINE__, __VA_ARGS__) : (void)0)
# define ProfBeginLockWait(...)    tmStartWaitForLock(0, 0, __VA_ARGS__)
# define ProfEndLockWait(...)      tmEndWaitForLock(0)
# define ProfLockTake(...)         tmAcquiredLock(0, 0, __VA_ARGS__)
# define ProfLockDrop(...)         tmReleasedLock(0, __VA_ARGS__)
# define ProfColor(color)          tmZoneColorSticky(color)
#endif

////////////////////////////////
//~ tec: Remotery Profile Defines
#if PROFILE_REMOTERY
global Remotery* __g_rmt = 0;

#define REMOTERY_BUFFER_SIZE 256
#define REMOTERY_VA_ARGS_TO_STRING(buf, ...) \
char buf[REMOTERY_BUFFER_SIZE]; \
snprintf(buf, REMOTERY_BUFFER_SIZE, __VA_ARGS__);

//#define ProfBegin(name) rmt_BeginCPUSampleStr(name, 0)
#define ProfBegin(...) do { REMOTERY_VA_ARGS_TO_STRING(buf, __VA_ARGS__); rmt_BeginCPUSampleStr(buf, 0); } while (0)
#define ProfBeginDynamic(name) rmt_BeginCPUSampleDynamic(name, 0)
# define ProfEnd(...) rmt_EndCPUSample()
# define ProfTick(...) rmt_MarkFrame()
# define ProfIsCapturing(...) (1)
# define ProfBeginCapture(...) (rmt_CreateGlobalInstance(&__g_rmt))
# define ProfEndCapture(...) (rmt_DestroyGlobalInstance(__g_rmt))
#define ProfThreadName(...) do { REMOTERY_VA_ARGS_TO_STRING(buf, __VA_ARGS__); rmt_SetCurrentThreadName(buf); } while (0)
//# define ProfThreadName(...) (REMOTERY_VA_ARGS_TO_STRING(__VA_ARGS__); rmt_SetCurrentThreadName(buffer);)
# define ProfMsg(...) do { REMOTERY_VA_ARGS_TO_STRING(buf, __VA_ARGS__); rmt_LogText(buf); } while (0)
# define ProfBeginLockWait(...)    (0)
# define ProfEndLockWait(...)      (0)
# define ProfLockTake(...)         (0)
# define ProfLockDrop(...)         (0)
# define ProfColor(...)            (0)
#endif

////////////////////////////////
//~ tec: Zeroify Undefined Defines

#if !defined(ProfBegin)
# define ProfBegin(...) (0)
# define ProfBeginDynamic(...) (0)
# define ProfEnd(...) (0)
# define ProfTick(...) (0)
# define ProfIsCapturing(...) (0)
# define ProfBeginCapture(...) (0)
# define ProfEndCapture(...) (0)
# define ProfThreadName(...) (0)
# define ProfMsg(...) (0)
# define ProfBeginLockWait(...)    (0)
# define ProfEndLockWait(...)      (0)
# define ProfLockTake(...)         (0)
# define ProfLockDrop(...)         (0)
# define ProfColor(...)            (0)
#endif

////////////////////////////////
//~ tec: Helper Wrappers

#define ProfBeginFunction() ProfBegin(this_function_name)
#define ProfScope(...) DeferLoop(ProfBeginDynamic(__VA_ARGS__), ProfEnd())

#endif //BASE_PROFILE_H
