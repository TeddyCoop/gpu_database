@echo off

set MSVC=1
set CLANG=0
set DEBUG=1
set RELEASE=0
set NO_METAGEN=1

:: compile defintions
set cl_common=     /I..\src\ /I..\src\third_party\CL /nologo /FC /Z7
set clang_common=  -I..\src\ -gcodeview -fdiagnostics-absolute-paths -Wall -Wno-unknown-warning-option -Wno-missing-braces -Wno-unused-function -Wno-writable-strings -Wno-unused-value -Wno-unused-variable -Wno-unused-local-typedef -Wno-deprecated-register -Wno-deprecated-declarations -Wno-unused-but-set-variable -Wno-single-bit-bitfield-constant-conversion -Xclang -flto-visibility-public-std -D_USE_MATH_DEFINES -Dstrdup=_strdup -Dgnu_printf=printf
set cl_debug=      call cl /Od %cl_common% %auto_compile_flags%
set cl_release=    call cl /O2 /DNDEBUG %cl_common% %auto_compile_flags%
set clang_debug=   call clang -g -O0 %clang_common% %auto_compile_flags%
set clang_release= call clang -g -O3 -DNDEBUG %clang_common% %auto_compile_flags%
:: /natvis:"%~dp0\src\natvis\base.natvis"
set cl_link=       /link ..\src\third_party\CL\OpenCL.lib
::/MANIFEST:EMBED /INCREMENTAL:NO  logo.res
set clang_link=    -fuse-ld=lld -Xlinker
::/MANIFEST:EMBED -Xlinker /natvis:"%~dp0\src\natvis\base.natvis" logo.res
set cl_out=        /out:
set clang_out=     -o

set link_dll=-DLL
if %MSVC%==1  set only_compile=/c
if %MSVC%==1  set EHsc=/EHsc
if %MSVC%==1  set rc=rc.exe
if %CLANG%==1  set only_compile=-c
if %CLANG%==1  set EHsc=
if %CLANG%==1  set rc=llvm-rc.exe

:: choose compile lines
if %MSVC%==1      set compile_debug=%cl_debug%
if %MSVC%==1      set compile_release=%cl_release%
if %MSVC%==1      set compile_link=%cl_link%
if %MSVC%==1      set out=%cl_out%
if %CLANG%==1      set compile_debug=%cl_debug%
if %CLANG%==1      set compile_release=%cl_release%
if %CLANG%==1      set compile_link=%cl_link%
if %CLANG%==1      set out=%cl_out%
if %DEBUG%==1     set compile=%compile_debug%
if %RELEASE%==1   set compile=%compile_release%

:: prep dirs
if not exist build mkdir build

:: build and run metagen
:: if %NO_METAGEN%==1 echo [skipping metagen]
:: if not %NO_METAGEN%==1 (
::  pushd build
::  %compile_debug% ..\src\metagen\metagen_main.c %compile_link% %out%metagen.exe || exit /b 1
::  metagen.exe || exit /b 1
:: 	popd
::)

:: pushd build
:: metagen.exe || exit /b 1
:: popd

:: build
pushd build
%compile% ..\src\main.c %compile_link% %out%gdb.exe|| exit /b 1
popd

:: unset
for %%a in (%*) do set "%%a=0"
set lz=
set compile=
set compile_link=
set out=
set msvc=
set debug=