# HyBBX compiler and linker hardening (HYBBX_HARDENING=ON)

function(hybbx_apply_hardening target)
    if(NOT HYBBX_HARDENING)
        return()
    endif()

    if(HYBBX_SUPPORTED_C_FLAGS)
        target_compile_options(${target} PRIVATE ${HYBBX_SUPPORTED_C_FLAGS})
    endif()

    if(HYBBX_WARNINGS_AS_ERRORS)
        target_compile_options(${target} PRIVATE -Werror)
    endif()

    if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
        if(NOT HYBBX_PLATFORM_AMIGAOS)
            target_compile_definitions(${target} PRIVATE _FORTIFY_SOURCE=2)
        endif()
    endif()

    if(NOT HYBBX_PLATFORM_AMIGAOS AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
        target_compile_options(${target} PRIVATE -fPIC)
        if(HYBBX_COMPILER_GCC OR HYBBX_COMPILER_CLANG)
            target_link_options(${target} PRIVATE
                -Wl,-z,relro
                -Wl,-z,now
            )
        endif()
    endif()
endfunction()

function(hybbx_apply_hardening_executable target)
    hybbx_apply_hardening(${target})

    if(NOT HYBBX_HARDENING OR HYBBX_PLATFORM_AMIGAOS)
        return()
    endif()

    if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND (HYBBX_COMPILER_GCC OR HYBBX_COMPILER_CLANG))
        target_compile_options(${target} PRIVATE -fPIE)
        target_link_options(${target} PRIVATE -pie)
    endif()
endfunction()
