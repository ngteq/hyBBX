# HyBBX toolchain detection and portable warning flags.
# Primary compiler: GCC. Also tested with Clang/LLVM.

include(CheckCCompilerFlag)

set(HYBBX_COMPILER_GCC FALSE)
set(HYBBX_COMPILER_CLANG FALSE)
set(HYBBX_COMPILER_APPLECLANG FALSE)

if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    set(HYBBX_COMPILER_GCC TRUE)
elseif(CMAKE_C_COMPILER_ID STREQUAL "Clang")
    if(CMAKE_C_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
        message(FATAL_ERROR "HyBBX requires GCC or Clang; MSVC is not supported.")
    endif()
    set(HYBBX_COMPILER_CLANG TRUE)
elseif(CMAKE_C_COMPILER_ID STREQUAL "AppleClang")
    set(HYBBX_COMPILER_CLANG TRUE)
    set(HYBBX_COMPILER_APPLECLANG TRUE)
else()
    message(WARNING
        "HyBBX is developed with GCC (primary) and Clang. "
        "Compiler '${CMAKE_C_COMPILER_ID}' is untested.")
endif()

function(hybbx_probe_c_flag flag out_var)
    string(REGEX REPLACE "[^a-zA-Z0-9]+" "_" flag_id "${flag}")
    set(cache_var "HYBBX_C_FLAG_${flag_id}")
    if(NOT DEFINED ${cache_var})
        check_c_compiler_flag("${flag}" ${cache_var})
    endif()
    if(${cache_var})
        set(${out_var} TRUE PARENT_SCOPE)
    else()
        set(${out_var} FALSE PARENT_SCOPE)
    endif()
endfunction()

function(hybbx_collect_supported_c_flags)
    set(_candidates
        -Wall
        -Wextra
        -Wpedantic
        -Wformat=2
        -Wstrict-prototypes
        -Wmissing-prototypes
        -Wwrite-strings
        -Wshadow
        -fno-strict-aliasing
        -fstack-protector-strong
    )

    if(HYBBX_COMPILER_GCC)
        list(APPEND _candidates
            -Wduplicated-cond
            -Wnull-dereference
            -Wlogical-op
            -Wformat-signedness
        )
        if(NOT CMAKE_C_COMPILER_VERSION VERSION_LESS 8.0)
            list(APPEND _candidates -fstack-clash-protection)
        endif()
    endif()

    if(HYBBX_COMPILER_CLANG AND NOT HYBBX_COMPILER_APPLECLANG)
        list(APPEND _candidates -Wno-gnu-zero-variadic-macro-arguments)
    endif()

    set(_supported "")
    foreach(flag IN LISTS _candidates)
        hybbx_probe_c_flag("${flag}" _ok)
        if(_ok)
            list(APPEND _supported "${flag}")
        endif()
    endforeach()

    set(HYBBX_SUPPORTED_C_FLAGS "${_supported}" CACHE INTERNAL
        "C warning/security flags supported by ${CMAKE_C_COMPILER_ID}")
endfunction()

hybbx_collect_supported_c_flags()

if(HYBBX_COMPILER_GCC)
    message(STATUS "HyBBX compiler: GCC ${CMAKE_C_COMPILER_VERSION} (primary)")
elseif(HYBBX_COMPILER_CLANG)
    message(STATUS "HyBBX compiler: Clang ${CMAKE_C_COMPILER_VERSION} (compatible)")
else()
    message(STATUS "HyBBX compiler: ${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}")
endif()
