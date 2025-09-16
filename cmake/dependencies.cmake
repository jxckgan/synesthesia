if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.12)
    cmake_policy(SET CMP0074 NEW)
endif()
if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.5)
    set(CMAKE_POLICY_DEFAULT_CMP0048 NEW)
endif()

# Include vendor configuration functions
include(cmake/vendor.cmake)

if(EXISTS "${GLFW_DIR}/CMakeLists.txt")
    message(STATUS "Using vendor GLFW from ${GLFW_DIR}")
    add_vendor_subdirectory(${GLFW_DIR} glfw)
    set(GLFW_TARGET glfw)
endif()

if(EXISTS "${JSON_DIR}/CMakeLists.txt")
    message(STATUS "Using vendor nlohmann/json from ${JSON_DIR}")
    add_vendor_subdirectory(${JSON_DIR} nlohmann_json)
endif()

if(EXISTS "${PORTAUDIO_DIR}/CMakeLists.txt")
    message(STATUS "Using vendor PortAudio from ${PORTAUDIO_DIR}")

    set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "Minimum policy version for CMake" FORCE)

    add_vendor_subdirectory(${PORTAUDIO_DIR} portaudio)
    if(TARGET PortAudio::PortAudio)
        set(PORTAUDIO_TARGET PortAudio::PortAudio)
    elseif(TARGET portaudio)
        set(PORTAUDIO_TARGET portaudio)
    else()
        message(FATAL_ERROR "PortAudio target not found in ${PORTAUDIO_DIR}")
    endif()
endif()