set(ARM64_DETECTED FALSE)
set(NEON_AVAILABLE FALSE)

if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64|ARM64")
    set(ARM64_DETECTED TRUE)
    message(STATUS "ARM64 architecture detected: ${CMAKE_SYSTEM_PROCESSOR}")

    if(ENABLE_NEON_OPTIMISATIONS)
        include(CheckCXXCompilerFlag)
        check_cxx_compiler_flag("-march=armv8-a" COMPILER_SUPPORTS_ARMV8)

        if(COMPILER_SUPPORTS_ARMV8)
            set(NEON_AVAILABLE TRUE)
            message(STATUS "ARM NEON optimisations enabled")
            add_definitions(-DUSE_NEON_OPTIMISATIONS)
        else()
            message(WARNING "Compiler does not support ARM NEON extensions")
        endif()
    else()
        message(STATUS "ARM NEON optimisations disabled by user")
    endif()
elseif(APPLE AND CMAKE_OSX_ARCHITECTURES MATCHES "arm64")
    set(ARM64_DETECTED TRUE)
    set(NEON_AVAILABLE TRUE)
    message(STATUS "Apple Silicon (ARM64) detected via CMAKE_OSX_ARCHITECTURES")
    if(ENABLE_NEON_OPTIMISATIONS)
        message(STATUS "ARM NEON optimisations enabled for Apple Silicon")
        add_definitions(-DUSE_NEON_OPTIMISATIONS)
    endif()
endif()

function(add_neon_sources)
    if(NEON_AVAILABLE AND ENABLE_NEON_OPTIMISATIONS)
        list(APPEND SOURCES
            ${SRC_DIR}/fft/fft_processor_neon.cpp
            ${SRC_DIR}/colour/colour_mapper_neon.cpp
        )
        set(SOURCES ${SOURCES} PARENT_SCOPE)
        message(STATUS "Added NEON-optimised source files to build")
    endif()
endfunction()

function(apply_neon_optimisations)
    if(APPLE AND NEON_AVAILABLE AND ENABLE_NEON_OPTIMISATIONS)
        target_compile_options(${EXECUTABLE_NAME} PRIVATE
            "-mcpu=apple-m1"
            "-mtune=apple-m1"
        )

        set_source_files_properties(
            ${SRC_DIR}/fft/fft_processor_neon.cpp
            ${SRC_DIR}/colour/colour_mapper_neon.cpp
            PROPERTIES COMPILE_FLAGS "-O3 -ffast-math -mcpu=apple-m1 -mtune=apple-m1"
        )

        message(STATUS "Applied NEON-specific compiler optimisations")
    endif()
endfunction()