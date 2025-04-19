# GDB
 
A GPU Powered SQL database.

## Setup Instructions
*Currently, only Windows x64 is supported*

### Install MSVC & Windows SDK

To compile this project, 'Microsoft C/C++ Build Tools v15 (2017) or later' is needed, for both the Windows SQK and MSVC compiler and linker.
If the Windows SQK is install, you may build with Clang (although it has not been tested)

A copy of OpenCL.lib must be placed in 'src\third_party\CL', any version past 2.0 will work. The SQK can be found [here](https://github.com/KhronosGroup/OpenCL-SDK). 

### Build Environment Setup

Building this project can be done so in a command line with the ability to call MSVC or Clang from the command line.

This can be done by calling 'vcvarsall.bat x64' in the command prompt.

To be sure that the MSVC compiler is accessible from the command line, type:
```
cl
```

If all is setup correctly, your output should look similar to:
```
Microsoft (R) C/C++ Optimizing Compiler Version 19.41.34120 for x64
Copyright (C) Microsoft Corporation.  All rights reserved.

usage: cl [ option... ] filename... [ /link linkoption... ]
```

### Build

Within the terminal, nagivate to the root directory of the codebase, then run 'build.bat'

If no errors are present, a folder named 'build' should be created in the root folder. There you will find a 'gdb.exe'

## Roadmap
- More GPU APIs (CUDA/Vulkan)
- Mutli-GPU support
- Complete SQL parser
  - Other database languages could be added
- Support more import/export formats