# Ninja must match the compiler's actual /showIncludes bytes. CMake's Windows
# code-page conversion can corrupt a localized prefix when cl emits UTF-8.
if(MSVC AND CMAKE_GENERATOR MATCHES "Ninja")
  set(_sdr_probe "${CMAKE_BINARY_DIR}/CMakeFiles/sdr-include-probe.c")
  file(WRITE "${_sdr_probe}" "#include <stddef.h>\n")
  execute_process(
    COMMAND "${CMAKE_CXX_COMPILER}" /nologo /showIncludes /TC /c "${_sdr_probe}"
      "/Fo${CMAKE_BINARY_DIR}/CMakeFiles/sdr-include-probe.obj"
    ENCODING NONE
    OUTPUT_VARIABLE _sdr_includes ERROR_VARIABLE _sdr_include_errors
    RESULT_VARIABLE _sdr_include_result)
  string(REGEX MATCH "(^|\n)([^\r\n]+: +)[A-Za-z]:[\\/]" _sdr_prefix "${_sdr_includes}")
  if(NOT _sdr_include_result EQUAL 0 OR NOT _sdr_prefix)
    message(FATAL_ERROR "Cannot detect MSVC include dependencies: ${_sdr_include_errors}")
  endif()
  set(CMAKE_CL_SHOWINCLUDES_PREFIX "${CMAKE_MATCH_2}")
  unset(_sdr_probe)
  unset(_sdr_includes)
  unset(_sdr_include_errors)
  unset(_sdr_include_result)
  unset(_sdr_prefix)
endif()
