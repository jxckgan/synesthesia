cmake_minimum_required(VERSION 3.12)
project(Synesthesia)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vendor/imgui")
set(KISSFFT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vendor/kissfft")
set(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(GLFW_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vendor/glfw")
set(PORTAUDIO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vendor/portaudio")

option(BUILD_MACOS_BUNDLE "Build as macOS .app bundle" OFF)

if(WIN32)
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        string(APPEND CMAKE_CXX_FLAGS_RELEASE " /O2 /GL")
        string(APPEND CMAKE_EXE_LINKER_FLAGS_RELEASE " /LTCG /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")
    endif()

    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/meta/icon/app.ico
        ${CMAKE_BINARY_DIR}/app.ico COPYONLY)
    configure_file(${SRC_DIR}/renderers/dx12/resource.h
        ${CMAKE_BINARY_DIR}/resource.h COPYONLY)
endif()

if(EXISTS "${GLFW_DIR}/CMakeLists.txt")
    message(STATUS "Using vendor GLFW from ${GLFW_DIR}")
    add_subdirectory(${GLFW_DIR})
    set(GLFW_TARGET glfw)
endif()

if(EXISTS "${PORTAUDIO_DIR}/CMakeLists.txt")
    message(STATUS "Using vendor PortAudio from ${PORTAUDIO_DIR}")
    add_subdirectory(${PORTAUDIO_DIR})
    
    if(TARGET PortAudio::PortAudio)
        set(PORTAUDIO_TARGET PortAudio::PortAudio)
    elseif(TARGET portaudio)
        set(PORTAUDIO_TARGET portaudio)
    else()
        message(FATAL_ERROR "PortAudio target not found in ${PORTAUDIO_DIR}")
    endif()
endif()

set(SOURCES
    ${SRC_DIR}/audio/audio_input.cpp
    ${SRC_DIR}/colour/colour_mapper.cpp
    ${SRC_DIR}/fft/fft_processor.cpp
    ${SRC_DIR}/ui.cpp
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${KISSFFT_DIR}/kiss_fft.c
)

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
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wformat -O3 -ffast-math -march=native")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wformat")
    set(OBJC_FLAGS "-ObjC++ -fobjc-weak -fobjc-arc")
endif()

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

target_include_directories(synesthesia PRIVATE
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${KISSFFT_DIR}
    ${SRC_DIR}
    ${SRC_DIR}/audio
    ${SRC_DIR}/colour
    ${SRC_DIR}/fft
    ${CMAKE_BINARY_DIR}
    /usr/local/include
    /opt/homebrew/include
    /opt/local/include
)

if(APPLE)
    find_library(METAL_FRAMEWORK Metal REQUIRED)
    find_library(METALKIT_FRAMEWORK MetalKit REQUIRED)
    find_library(COCOA_FRAMEWORK Cocoa REQUIRED)
    find_library(IOKIT_FRAMEWORK IOKit REQUIRED)
    find_library(COREVIDEO_FRAMEWORK CoreVideo REQUIRED)
    find_library(QUARTZCORE_FRAMEWORK QuartzCore REQUIRED)

    target_link_libraries(synesthesia
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
    target_link_libraries(synesthesia
        ${GLFW_TARGET}
        ${PORTAUDIO_TARGET}
        ${DX12_LIBS}
    )
else()
    target_link_libraries(synesthesia
        ${GLFW_TARGET}
        ${PORTAUDIO_TARGET}
        GL
    )
endif()

set_target_properties(synesthesia PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

if(APPLE AND BUILD_MACOS_BUNDLE)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/meta/Info.plist.in ${CMAKE_BINARY_DIR}/Info.plist)
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/meta/entitlements.plist
        ${CMAKE_BINARY_DIR}/entitlements.plist
        COPYONLY
    )

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