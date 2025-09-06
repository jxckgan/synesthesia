cmake_minimum_required(VERSION 3.13)

set(SYNESTHESIA_VERSION_MAJOR 2025)
set(SYNESTHESIA_VERSION_MINOR 09)
set(SYNESTHESIA_VERSION_PATCH 06)
set(SYNESTHESIA_VERSION "${SYNESTHESIA_VERSION_MAJOR}.${SYNESTHESIA_VERSION_MINOR}.${SYNESTHESIA_VERSION_PATCH}")

project(Synesthesia VERSION ${SYNESTHESIA_VERSION})

set(EXECUTABLE_NAME "Synesthesia")
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vendor/imgui")
set(IMPLOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vendor/implot")
set(KISSFFT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vendor/kissfft")
set(JSON_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vendor/json")
set(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(GLFW_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vendor/glfw")
set(PORTAUDIO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vendor/portaudio")

option(BUILD_MACOS_BUNDLE "Build as macOS .app bundle" OFF)
option(ENABLE_NEON_OPTIMIZATIONS "Enable ARM NEON SIMD optimizations" ON)
option(ENABLE_API_SERVER "Enable cross-application colour streaming API (macOS only)" OFF)

# Automatically enable API server for macOS, disable for other platforms
if(APPLE)
    set(ENABLE_API_SERVER ON)
    message(STATUS "API server enabled for macOS")
elseif(ENABLE_API_SERVER)
    message(WARNING "API server is only supported on macOS, disabling")
    set(ENABLE_API_SERVER OFF)
endif()

# Detect ARM64 architecture and NEON support
set(ARM64_DETECTED FALSE)
set(NEON_AVAILABLE FALSE)

if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64|ARM64")
    set(ARM64_DETECTED TRUE)
    message(STATUS "ARM64 architecture detected: ${CMAKE_SYSTEM_PROCESSOR}")
    
    if(ENABLE_NEON_OPTIMIZATIONS)
        # Check for NEON support
        include(CheckCXXCompilerFlag)
        check_cxx_compiler_flag("-march=armv8-a" COMPILER_SUPPORTS_ARMV8)
        
        if(COMPILER_SUPPORTS_ARMV8)
            set(NEON_AVAILABLE TRUE)
            message(STATUS "ARM NEON optimizations enabled")
            add_definitions(-DUSE_NEON_OPTIMIZATIONS)
        else()
            message(WARNING "Compiler does not support ARM NEON extensions")
        endif()
    else()
        message(STATUS "ARM NEON optimizations disabled by user")
    endif()
elseif(APPLE AND CMAKE_OSX_ARCHITECTURES MATCHES "arm64")
    set(ARM64_DETECTED TRUE)
    set(NEON_AVAILABLE TRUE)
    message(STATUS "Apple Silicon (ARM64) detected via CMAKE_OSX_ARCHITECTURES")
    if(ENABLE_NEON_OPTIMIZATIONS)
        message(STATUS "ARM NEON optimizations enabled for Apple Silicon")
        add_definitions(-DUSE_NEON_OPTIMIZATIONS)
    endif()
endif()

# Configure version header
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/src/version.h.in"
    "${CMAKE_BINARY_DIR}/version.h"
    @ONLY
)

# Windows-specific configurations
if(WIN32)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/meta/icon/app.ico
        ${CMAKE_BINARY_DIR}/app.ico COPYONLY)
    configure_file(${SRC_DIR}/renderers/dx12/resource.h
        ${CMAKE_BINARY_DIR}/resource.h COPYONLY)
endif()

# Add vendor subdirectories
if(EXISTS "${GLFW_DIR}/CMakeLists.txt")
    message(STATUS "Using vendor GLFW from ${GLFW_DIR}")
    add_subdirectory(${GLFW_DIR})
    set(GLFW_TARGET glfw)
endif()

if(EXISTS "${JSON_DIR}/CMakeLists.txt")
    message(STATUS "Using vendor nlohmann/json from ${JSON_DIR}")
    add_subdirectory(${JSON_DIR})
endif()

if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.12)
    cmake_policy(SET CMP0074 NEW)
endif()
if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.5)
    set(CMAKE_POLICY_DEFAULT_CMP0048 NEW)
endif()

if(EXISTS "${PORTAUDIO_DIR}/CMakeLists.txt")
    message(STATUS "Using vendor PortAudio from ${PORTAUDIO_DIR}")
    
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "Minimum policy version for CMake" FORCE)
    
    add_subdirectory(${PORTAUDIO_DIR})
    if(TARGET PortAudio::PortAudio)
        set(PORTAUDIO_TARGET PortAudio::PortAudio)
    elseif(TARGET portaudio)
        set(PORTAUDIO_TARGET portaudio)
    else()
        message(FATAL_ERROR "PortAudio target not found in ${PORTAUDIO_DIR}")
    endif()
endif()

# Collect source files
set(SOURCES
    ${SRC_DIR}/main.cpp
    ${SRC_DIR}/zero_crossing/zero_crossing.cpp
    ${SRC_DIR}/audio/audio_input.cpp
    ${SRC_DIR}/audio/audio_processor.cpp
    ${SRC_DIR}/colour/colour_mapper.cpp
    ${SRC_DIR}/fft/fft_processor.cpp
    ${SRC_DIR}/ui/controls/controls.cpp
    ${SRC_DIR}/ui/device_manager/device_manager.cpp
    ${SRC_DIR}/ui/updating/update.cpp
    ${SRC_DIR}/ui/smoothing/smoothing.cpp
    ${SRC_DIR}/ui/styling/styling.cpp
    ${SRC_DIR}/ui/spectrum_analyser/spectrum_analyser.cpp
    ${SRC_DIR}/ui.cpp
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMPLOT_DIR}/implot.cpp
    ${IMPLOT_DIR}/implot_items.cpp
    ${KISSFFT_DIR}/kiss_fft.c
    ${KISSFFT_DIR}/kiss_fftr.c
)

# Add API sources if enabled
if(ENABLE_API_SERVER)
    list(APPEND SOURCES
        ${SRC_DIR}/api/common/serialisation.cpp
        ${SRC_DIR}/api/common/transport.cpp
        ${SRC_DIR}/api/server/api_server.cpp
        ${SRC_DIR}/api/synesthesia_api_integration.cpp
    )
    message(STATUS "Added API server sources to build")
endif()

# Add CLI sources only for macOS
if(APPLE)
    list(APPEND SOURCES
        ${SRC_DIR}/cli/cli.cpp
        ${SRC_DIR}/cli/headless.cpp
    )
    message(STATUS "Added CLI sources to build for macOS")
endif()

# Add NEON-optimised sources for ARM64 platforms
if(NEON_AVAILABLE AND ENABLE_NEON_OPTIMIZATIONS)
    list(APPEND SOURCES
        ${SRC_DIR}/fft/fft_processor_neon.cpp
        ${SRC_DIR}/colour/colour_mapper_neon.cpp
    )
    message(STATUS "Added NEON-optimised source files to build")
endif()

# Platform-specific source additions
if(WIN32)
    message(STATUS "Configuring for Windows (DirectX 12)")
    list(APPEND SOURCES
        ${SRC_DIR}/renderers/dx12/main.cpp
        ${IMGUI_DIR}/backends/imgui_impl_dx12.cpp
        ${IMGUI_DIR}/backends/imgui_impl_win32.cpp
        ${SRC_DIR}/renderers/dx12/app.rc
    )
    set(DX12_LIBS d3d12 dxgi d3dcompiler)
elseif(APPLE)
    message(STATUS "Configuring for macOS (Metal)")
    list(APPEND SOURCES
        ${SRC_DIR}/renderers/metal/main.mm
        ${IMGUI_DIR}/backends/imgui_impl_metal.mm
        ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    )
    set(OBJC_FLAGS "-ObjC++ -fobjc-arc -fobjc-weak")
endif()

# Create the executable
if(WIN32)
    add_executable(${EXECUTABLE_NAME} WIN32 ${SOURCES})
elseif(APPLE)
    if(BUILD_MACOS_BUNDLE)
        add_executable(${EXECUTABLE_NAME} MACOSX_BUNDLE ${SOURCES})
    else()
        add_executable(${EXECUTABLE_NAME} ${SOURCES})
    endif()
else()
    add_executable(${EXECUTABLE_NAME} ${SOURCES})
endif()

# Include directories
target_include_directories(${EXECUTABLE_NAME} PRIVATE
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${IMPLOT_DIR}
    ${KISSFFT_DIR}
    ${SRC_DIR}
    ${SRC_DIR}/audio
    ${SRC_DIR}/colour
    ${SRC_DIR}/fft
    ${SRC_DIR}/ui/controls
    ${SRC_DIR}/ui/device_manager
    ${SRC_DIR}/ui/updating
    ${SRC_DIR}/ui/smoothing
    ${SRC_DIR}/ui/styling
    ${SRC_DIR}/ui/spectrum_analyser
    ${SRC_DIR}/zero_crossing
    ${SRC_DIR}/cli
    ${CMAKE_BINARY_DIR}
    /usr/local/include
    /opt/homebrew/include
    /opt/local/include
)

# Add API include directories if enabled
if(ENABLE_API_SERVER)
    target_include_directories(${EXECUTABLE_NAME} PRIVATE
        ${SRC_DIR}/api
        ${SRC_DIR}/api/common
        ${SRC_DIR}/api/server
        ${SRC_DIR}/api/client
        ${SRC_DIR}/api/protocol
    )
endif()

# Define version macros for the target
target_compile_definitions(${EXECUTABLE_NAME} PRIVATE
    SYNESTHESIA_VERSION_MAJOR=${SYNESTHESIA_VERSION_MAJOR}
    SYNESTHESIA_VERSION_MINOR=${SYNESTHESIA_VERSION_MINOR}
    SYNESTHESIA_VERSION_PATCH=${SYNESTHESIA_VERSION_PATCH}
    SYNESTHESIA_VERSION_STRING="${SYNESTHESIA_VERSION}"
)

# Add API preprocessor definition if enabled
if(ENABLE_API_SERVER)
    target_compile_definitions(${EXECUTABLE_NAME} PRIVATE ENABLE_API_SERVER)
endif()

# Platform-specific compile options
if(WIN32)
    target_compile_options(${EXECUTABLE_NAME} PRIVATE 
        $<$<CONFIG:Release>:/O2>
        $<$<CONFIG:Release>:/GL>
    )
    target_link_options(${EXECUTABLE_NAME} PRIVATE 
        $<$<CONFIG:Release>:/LTCG>
        $<$<CONFIG:Release>:/SUBSYSTEM:WINDOWS>
        $<$<CONFIG:Release>:/ENTRY:mainCRTStartup>
    )
elseif(APPLE)
    target_compile_options(${EXECUTABLE_NAME} PRIVATE
        "-Wall" "-Wformat" "-O3" "-ffast-math" "-march=native"
    )
    
    # Add NEON-specific optimizations for ARM64
    if(NEON_AVAILABLE AND ENABLE_NEON_OPTIMIZATIONS)
        target_compile_options(${EXECUTABLE_NAME} PRIVATE
            "-mcpu=apple-m1"  # Optimise for Apple Silicon
            "-mtune=apple-m1"
        )
        
        # Set specific compile flags for NEON source files
        set_source_files_properties(
            ${SRC_DIR}/fft/fft_processor_neon.cpp
            ${SRC_DIR}/colour/colour_mapper_neon.cpp
            PROPERTIES COMPILE_FLAGS "-O3 -ffast-math -mcpu=apple-m1 -mtune=apple-m1"
        )
        
        message(STATUS "Applied NEON-specific compiler optimizations")
    endif()
    
    set_source_files_properties(${SRC_DIR}/renderers/metal/main.mm PROPERTIES COMPILE_FLAGS "${OBJC_FLAGS}")
    set_source_files_properties(${IMGUI_DIR}/backends/imgui_impl_metal.mm PROPERTIES COMPILE_FLAGS "${OBJC_FLAGS}")
endif()

# Link libraries
if(APPLE)
    find_library(METAL_FRAMEWORK Metal REQUIRED)
    find_library(METALKIT_FRAMEWORK MetalKit REQUIRED)
    find_library(COCOA_FRAMEWORK Cocoa REQUIRED)
    find_library(IOKIT_FRAMEWORK IOKit REQUIRED)
    find_library(COREVIDEO_FRAMEWORK CoreVideo REQUIRED)
    find_library(QUARTZCORE_FRAMEWORK QuartzCore REQUIRED)

    target_link_libraries(${EXECUTABLE_NAME} PRIVATE
        ${METAL_FRAMEWORK}
        ${METALKIT_FRAMEWORK}
        ${COCOA_FRAMEWORK}
        ${IOKIT_FRAMEWORK}
        ${COREVIDEO_FRAMEWORK}
        ${QUARTZCORE_FRAMEWORK}
        ${GLFW_TARGET}
        ${PORTAUDIO_TARGET}
        nlohmann_json::nlohmann_json
        m
    )
elseif(WIN32)
    target_link_libraries(${EXECUTABLE_NAME} PRIVATE
        ${GLFW_TARGET}
        ${PORTAUDIO_TARGET}
        ${DX12_LIBS}
        nlohmann_json::nlohmann_json
    )
else()
    target_link_libraries(${EXECUTABLE_NAME} PRIVATE
        ${GLFW_TARGET}
        ${PORTAUDIO_TARGET}
        GL
        nlohmann_json::nlohmann_json
    )
endif()

set_target_properties(${EXECUTABLE_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

# macOS Bundle Configuration
if(APPLE AND BUILD_MACOS_BUNDLE)
    set_target_properties(${EXECUTABLE_NAME} PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_BINARY_DIR}/Info.plist"
        MACOSX_BUNDLE_ICON_FILE "app.icns"
        MACOSX_BUNDLE_BUNDLE_NAME "Synesthesia"
        MACOSX_BUNDLE_BUNDLE_VERSION "${SYNESTHESIA_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${SYNESTHESIA_VERSION}"
        MACOSX_BUNDLE_LONG_VERSION_STRING "Synesthesia ${SYNESTHESIA_VERSION}"
        MACOSX_BUNDLE_COPYRIGHT "Copyright © 2025 Jack Gannon | MIT License."
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.jackgannon.Synesthesia"
        MACOSX_BUNDLE_EXECUTABLE_NAME "${EXECUTABLE_NAME}"
        RESOURCE "${ICON_SRC}"
    )

    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/meta/Info.plist.in ${CMAKE_BINARY_DIR}/Info.plist)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/meta/entitlements.plist ${CMAKE_BINARY_DIR}/entitlements.plist COPYONLY)
    
    set(ICON_SRC "${CMAKE_CURRENT_SOURCE_DIR}/meta/icon/app.icns")
    set(ICON_DEST "${CMAKE_BINARY_DIR}/${EXECUTABLE_NAME}.app/Contents/Resources/app.icns")

    add_custom_command(TARGET ${EXECUTABLE_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/${EXECUTABLE_NAME}.app/Contents/Resources"
        COMMAND ${CMAKE_COMMAND} -E copy "${ICON_SRC}" "${ICON_DEST}"
        COMMENT "Copying app icon to bundle resources"
    )
endif()

# Installation Rules
if(WIN32 AND CMAKE_BUILD_TYPE STREQUAL "Release")
    install(TARGETS ${EXECUTABLE_NAME}
        RUNTIME DESTINATION bin
    )
    install(FILES 
        $<TARGET_RUNTIME_DLLS:${EXECUTABLE_NAME}>
        DESTINATION bin
    )
elseif(APPLE)
    install(TARGETS ${EXECUTABLE_NAME} BUNDLE DESTINATION ".")
else()
    install(TARGETS ${EXECUTABLE_NAME}
        RUNTIME DESTINATION bin
    )
endif()
