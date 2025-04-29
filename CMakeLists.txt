cmake_minimum_required(VERSION 3.13)
project(Synesthesia)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vendor/imgui")
set(KISSFFT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vendor/kissfft")
set(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(GLFW_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vendor/glfw")
set(PORTAUDIO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vendor/portaudio")

option(BUILD_MACOS_BUNDLE "Build as macOS .app bundle" OFF)

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
    ${SRC_DIR}/zero_crossing/zero_crossing.cpp
    ${SRC_DIR}/audio/audio_input.cpp
    ${SRC_DIR}/audio/audio_processor.cpp
    ${SRC_DIR}/colour/colour_mapper.cpp
    ${SRC_DIR}/fft/fft_processor.cpp
    ${SRC_DIR}/ui/smoothing.cpp
    ${SRC_DIR}/ui.cpp
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${KISSFFT_DIR}/kiss_fft.c
    ${KISSFFT_DIR}/kiss_fftr.c
)

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
    add_executable(synesthesia WIN32 ${SOURCES})
elseif(APPLE)
    if(BUILD_MACOS_BUNDLE)
        add_executable(synesthesia MACOSX_BUNDLE ${SOURCES})
    else()
        add_executable(synesthesia ${SOURCES})
    endif()
else()
    add_executable(synesthesia ${SOURCES})
endif()

# Include directories
target_include_directories(synesthesia PRIVATE
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${KISSFFT_DIR}
    ${SRC_DIR}
    ${SRC_DIR}/audio
    ${SRC_DIR}/colour
    ${SRC_DIR}/fft
    ${SRC_DIR}/ui
    ${SRC_DIR}/zero_crossing
    ${CMAKE_BINARY_DIR}
    /usr/local/include
    /opt/homebrew/include
    /opt/local/include
)

# Platform-specific compile options
if(WIN32)
    target_compile_options(synesthesia PRIVATE 
        $<$<CONFIG:Release>:/O2>
        $<$<CONFIG:Release>:/GL>
    )
    target_link_options(synesthesia PRIVATE 
        $<$<CONFIG:Release>:/LTCG>
        $<$<CONFIG:Release>:/SUBSYSTEM:WINDOWS>
        $<$<CONFIG:Release>:/ENTRY:mainCRTStartup>
    )
elseif(APPLE)
    target_compile_options(synesthesia PRIVATE
        "-Wall" "-Wformat" "-O3" "-ffast-math" "-march=native"
    )
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

    target_link_libraries(synesthesia PRIVATE
        ${METAL_FRAMEWORK}
        ${METALKIT_FRAMEWORK}
        ${COCOA_FRAMEWORK}
        ${IOKIT_FRAMEWORK}
        ${COREVIDEO_FRAMEWORK}
        ${QUARTZCORE_FRAMEWORK}
        ${GLFW_TARGET}
        ${PORTAUDIO_TARGET}
        m
    )
elseif(WIN32)
    target_link_libraries(synesthesia PRIVATE
        ${GLFW_TARGET}
        ${PORTAUDIO_TARGET}
        ${DX12_LIBS}
    )
else()
    target_link_libraries(synesthesia PRIVATE
        ${GLFW_TARGET}
        ${PORTAUDIO_TARGET}
        GL
    )
endif()

set_target_properties(synesthesia PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

# macOS Bundle Configuration
if(APPLE AND BUILD_MACOS_BUNDLE)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/meta/Info.plist.in ${CMAKE_BINARY_DIR}/Info.plist)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/meta/entitlements.plist ${CMAKE_BINARY_DIR}/entitlements.plist COPYONLY)
    
    set(ICON_SRC "${CMAKE_CURRENT_SOURCE_DIR}/meta/icon/app.icns")
    set(ICON_DEST "${CMAKE_BINARY_DIR}/synesthesia.app/Contents/Resources/app.icns")
    add_custom_command(TARGET synesthesia POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/synesthesia.app/Contents/Resources"
        COMMAND ${CMAKE_COMMAND} -E copy "${ICON_SRC}" "${ICON_DEST}"
        COMMENT "Copying app icon to bundle resources"
    )
    set_target_properties(synesthesia PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_BINARY_DIR}/Info.plist"
        MACOSX_BUNDLE_ICON_FILE "app.icns"
        MACOSX_BUNDLE_BUNDLE_NAME "Synesthesia"
        MACOSX_BUNDLE_BUNDLE_VERSION "1.0"
        MACOSX_BUNDLE_COPYRIGHT "Copyright (C) 2025 Jack Gannon | MIT License"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.jackgannon.Synesthesia"
        RESOURCE "${ICON_SRC}"
    )
endif()

# Installation Rules
if(WIN32 AND CMAKE_BUILD_TYPE STREQUAL "Release")
    install(TARGETS synesthesia
        RUNTIME DESTINATION bin
    )
    install(FILES 
        $<TARGET_RUNTIME_DLLS:synesthesia>
        DESTINATION bin
    )
elseif(APPLE)
    install(TARGETS synesthesia BUNDLE DESTINATION ".")
else()
    install(TARGETS synesthesia
        RUNTIME DESTINATION bin
    )
endif()