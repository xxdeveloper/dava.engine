include(Compiler/GNU)
__compiler_gnu(CXX)

if (WIN32)
  if(NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS 4.6)
    set(CMAKE_CXX_COMPILE_OPTIONS_VISIBILITY_INLINES_HIDDEN "-fno-keep-inline-dllexport")
  endif()
else()
  if(NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS 4.2)
    set(CMAKE_CXX_COMPILE_OPTIONS_VISIBILITY_INLINES_HIDDEN "-fvisibility-inlines-hidden")
  endif()
endif()

if(NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS 4.4)
  # Supported since 4.3
  set(CMAKE_CXX98_STANDARD_COMPILE_OPTION "-std=c++98")
  set(CMAKE_CXX98_EXTENSION_COMPILE_OPTION "-std=gnu++98")
endif()

if (NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS 4.7)
  set(CMAKE_CXX11_STANDARD_COMPILE_OPTION "-std=c++11")
  set(CMAKE_CXX11_EXTENSION_COMPILE_OPTION "-std=gnu++11")
elseif (NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS 4.4)
  # 4.3 supports 0x variants
  set(CMAKE_CXX11_STANDARD_COMPILE_OPTION "-std=c++0x")
  set(CMAKE_CXX11_EXTENSION_COMPILE_OPTION "-std=gnu++0x")
endif()

if (NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS 4.9)
  set(CMAKE_CXX14_STANDARD_COMPILE_OPTION "-std=c++14")
  set(CMAKE_CXX14_EXTENSION_COMPILE_OPTION "-std=gnu++14")
elseif (NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS 4.8)
  set(CMAKE_CXX14_STANDARD_COMPILE_OPTION "-std=c++1y")
  set(CMAKE_CXX14_EXTENSION_COMPILE_OPTION "-std=gnu++1y")
endif()

if(NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS 4.4)
  if (NOT CMAKE_CXX_COMPILER_FORCED)
    if (NOT CMAKE_CXX_STANDARD_COMPUTED_DEFAULT)
      message(FATAL_ERROR "CMAKE_CXX_STANDARD_COMPUTED_DEFAULT should be set for ${CMAKE_CXX_COMPILER_ID} (${CMAKE_CXX_COMPILER}) version ${CMAKE_CXX_COMPILER_VERSION}")
    endif()
    set(CMAKE_CXX_STANDARD_DEFAULT ${CMAKE_CXX_STANDARD_COMPUTED_DEFAULT})
  endif()
endif()

macro(cmake_record_cxx_compile_features)
  macro(_get_gcc_features std_version list)
    record_compiler_features(CXX "${std_version}" ${list})
  endmacro()

  set(_result 0)
  if (NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS 4.8)
    _get_gcc_features(${CMAKE_CXX14_STANDARD_COMPILE_OPTION} CMAKE_CXX14_COMPILE_FEATURES)
  endif()
  if (NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS 4.4)
    if (_result EQUAL 0)
      _get_gcc_features(${CMAKE_CXX11_STANDARD_COMPILE_OPTION} CMAKE_CXX11_COMPILE_FEATURES)
    endif()
    if (_result EQUAL 0)
      _get_gcc_features(${CMAKE_CXX98_STANDARD_COMPILE_OPTION} CMAKE_CXX98_COMPILE_FEATURES)
    endif()
  endif()
endmacro()
