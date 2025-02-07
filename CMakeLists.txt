cmake_minimum_required(VERSION 3.12)
project(Synesthesia)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vendor/imgui")
set(KISSFFT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vendor/kissfft")
set(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(GLFW_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vendor/glfw")
set(PORTAUDIO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vendor/portaudio")

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
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${KISSFFT_DIR}/kiss_fft.c
)

if(APPLE)
    list(APPEND SOURCES
        ${SRC_DIR}/metal/main.mm
        ${IMGUI_DIR}/backends/imgui_impl_metal.mm
    )
endif()

include_directories(
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${KISSFFT_DIR}
    ${SRC_DIR}
    ${SRC_DIR}/audio
    ${SRC_DIR}/colour
    ${SRC_DIR}/fft
    /usr/local/include
    /opt/homebrew/include
    /opt/local/include
)

if(APPLE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wformat -O3 -ffast-math -march=native")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wformat")
    set(OBJC_FLAGS "-ObjC++ -fobjc-weak -fobjc-arc")
endif()

if(APPLE)
    find_library(METAL_FRAMEWORK Metal REQUIRED)
    find_library(METALKIT_FRAMEWORK MetalKit REQUIRED)
    find_library(COCOA_FRAMEWORK Cocoa REQUIRED)
    find_library(IOKIT_FRAMEWORK IOKit REQUIRED)
    find_library(COREVIDEO_FRAMEWORK CoreVideo REQUIRED)
    find_library(QUARTZCORE_FRAMEWORK QuartzCore REQUIRED)
endif()

add_executable(synesthesia ${SOURCES})

if(APPLE)
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
else()
    target_link_libraries(synesthesia
        ${GLFW_TARGET}
        ${PORTAUDIO_TARGET}
        m
    )
endif()

set_target_properties(synesthesia PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

if(APPLE)
    option(BUILD_MACOS_APP "Build a macOS application bundle" OFF)
    if(BUILD_MACOS_APP)
        add_custom_target(app
            COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/synesthesia.app
            COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/meta/appify.sh -s ${CMAKE_BINARY_DIR}/synesthesia -i ${CMAKE_CURRENT_SOURCE_DIR}/meta/icon/app.icns
            DEPENDS synesthesia
            COMMENT "Creating macOS app bundle..."
        )
    endif()
endif()
