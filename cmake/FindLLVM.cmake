# Detect LLVM and set various variable to link against the different component of LLVM
#
# [Robin] NOTE: This is adapted from http://code.roadsend.com/pcc/export/796/trunk/rphp/cmake/modules/FindLLVM.cmake
# [Robin] We also search for clang.
# [Robin] Removed messages outputting the variables; added that to EnableLLVM.cmake instead.
# [Robin] Removed llvm-gcc/g++ stuff.
# [Robin] General cleanup, and renamed some of the output variables.
#
# NOTE: This is a modified version of the module originally found in the OpenGTL project
# at www.opengtl.org
#
# LLVM_FOUND        TRUE if LLVM and clang were found.
#
# LLVM_BIN_DIR      Directory with LLVM binaries.
# LLVM_LIB_DIR      Directory with LLVM library.
# LLVM_INCLUDE_DIR  Directory with LLVM include.
#
# LLVM_CFLAGS       Compiler flags needed to build a C program using LLVM headers.
# LLVM_CXXFLAGS     Compiler flags needed to build a C++ program using LLVM headers.
# LLVM_LDFLAGS      Linker flags for linking with the core LLVM libraries.
# LLVM_LIBS         Linker libraries for linking with the core LLVM libraries.
#
# LLVM_CONFIG_EXEC  Path of the llvm-config executable.
# LLVM_CLANG_EXEC   Path of the clang executable.
# LLVM_CLANGXX_EXEC Path of the clang++ executable.
#
# [Disabled for now. -Robin] LLVM_LIBS_JIT : ldflags needed to link against a LLVM JIT
# [Disabled for now. -Robin] LLVM_LIBS_JIT_OBJECTS : objects you need to add to your source when using LLVM JIT

if (LLVM_INCLUDE_DIR)
  set(LLVM_FOUND TRUE)

else ()

  find_program(LLVM_CONFIG_EXEC
      NAMES llvm-config
      PATHS /opt/local/bin /opt/llvm/bin
  )

  find_program(LLVM_CLANG_EXEC
      NAMES clang
      PATHS /opt/local/bin  /opt/llvm/bin
  )

  find_program(LLVM_CLANGXX_EXEC
      NAMES clang++
      PATHS /opt/local/bin  /opt/llvm/bin
  )

  set(LLVM_FOUND TRUE)

  if ( LLVM_CONFIG_EXEC )
      MESSAGE(STATUS "Found llvm-config at ${LLVM_CONFIG_EXEC}")
  else ()
      MESSAGE(STATUS "llvm-config not found")
      set(LLVM_FOUND FALSE)
  endif ()

  if ( LLVM_CLANG_EXEC )
      MESSAGE(STATUS "Found clang at ${LLVM_CLANG_EXEC}")
  else ()
      MESSAGE(STATUS "clang not found")
      set(LLVM_FOUND FALSE)
  endif ()

  if ( LLVM_CLANGXX_EXEC )
      MESSAGE(STATUS "Found clang++ at ${LLVM_CLANGXX_EXEC}")
  else ()
      MESSAGE(STATUS "clang++ not found")
      set(LLVM_FOUND FALSE)
  endif ()

  MACRO(FIND_LLVM_LIBS LLVM_CONFIG_EXEC _libname_ LIB_VAR OBJECT_VAR)
    exec_program( ${LLVM_CONFIG_EXEC} ARGS --libs ${_libname_}  OUTPUT_VARIABLE ${LIB_VAR} )
    STRING(REGEX MATCHALL "[^ ]*[.]o[ $]"  ${OBJECT_VAR} ${${LIB_VAR}})
    SEPARATE_ARGUMENTS(${OBJECT_VAR})
    STRING(REGEX REPLACE "[^ ]*[.]o[ $]" ""  ${LIB_VAR} ${${LIB_VAR}})
  ENDMACRO(FIND_LLVM_LIBS)

  exec_program(${LLVM_CONFIG_EXEC} ARGS --bindir     OUTPUT_VARIABLE LLVM_BIN_DIR)
  exec_program(${LLVM_CONFIG_EXEC} ARGS --libdir     OUTPUT_VARIABLE LLVM_LIB_DIR)
  exec_program(${LLVM_CONFIG_EXEC} ARGS --includedir OUTPUT_VARIABLE LLVM_INCLUDE_DIR)

  exec_program(${LLVM_CONFIG_EXEC} ARGS --cflags     OUTPUT_VARIABLE LLVM_CFLAGS )
  exec_program(${LLVM_CONFIG_EXEC} ARGS --cxxflags   OUTPUT_VARIABLE LLVM_CXXFLAGS )
  exec_program(${LLVM_CONFIG_EXEC} ARGS --ldflags    OUTPUT_VARIABLE LLVM_LDFLAGS )

  # llvm-config includes stuff we don't want.
  set(cflags_to_remove "-fno-exceptions" "-O." "-fomit-frame-pointer" "-stdlib=libc\\+\\+" "-D_DEBUG")

  foreach(f ${cflags_to_remove})
      STRING(REGEX REPLACE "${f}" "" LLVM_CFLAGS   "${LLVM_CFLAGS}")
      STRING(REGEX REPLACE "${f}" "" LLVM_CXXFLAGS "${LLVM_CXXFLAGS}")
  endforeach()

  set(llvm_libs "core bitreader bitwriter linker asmparser interpreter executionengine jit mcjit runtimedyld nativecodegen")
  exec_program(${LLVM_CONFIG_EXEC} ARGS --libs ${llvm_libs} OUTPUT_VARIABLE LLVM_LIBS)

  set(LLVM_LDFLAGS "${LLVM_LDFLAGS} -Wl,-rpath,${LLVM_LIB_DIR}")

  # FIND_LLVM_LIBS( ${LLVM_CONFIG_EXEC} "jit native" LLVM_LIBS_JIT LLVM_LIBS_JIT_OBJECTS )
  # STRING(REPLACE " -lLLVMCore -lLLVMSupport -lLLVMSystem" "" LLVM_LIBS_JIT ${LLVM_LIBS_JIT_RAW})

endif ()
