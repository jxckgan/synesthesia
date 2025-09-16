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
)

function(add_api_sources)
    if(ENABLE_API_SERVER)
        list(APPEND SOURCES
            ${SRC_DIR}/api/common/serialisation.cpp
            ${SRC_DIR}/api/common/transport.cpp
            ${SRC_DIR}/api/server/api_server.cpp
            ${SRC_DIR}/api/synesthesia_api_integration.cpp
        )
        set(SOURCES ${SOURCES} PARENT_SCOPE)
        message(STATUS "Added API server sources to build")
    endif()
endfunction()

function(configure_include_directories)
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
        /opt/homebrew/include
    )

    if(ENABLE_API_SERVER)
        target_include_directories(${EXECUTABLE_NAME} PRIVATE
            ${SRC_DIR}/api
            ${SRC_DIR}/api/common
            ${SRC_DIR}/api/server
            ${SRC_DIR}/api/client
            ${SRC_DIR}/api/protocol
        )
    endif()
endfunction()