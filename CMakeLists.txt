cmake_minimum_required(VERSION 3.20)
project(custom-loading-screen)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_GENERATOR_PLATFORM x86)

set(DETOURS_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/third_party/detours/include")
set(DETOURS_LIBRARY "${CMAKE_SOURCE_DIR}/third_party/detours/lib/detours.lib")

set(STB_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/third_party/stb")

file(GLOB_RECURSE SOURCES "src/*.cpp" "src/*.h")

add_library(custom-loading-screen SHARED ${SOURCES})

target_include_directories(custom-loading-screen PRIVATE ${DETOURS_INCLUDE_DIR} ${STB_INCLUDE_DIR})
target_link_libraries(custom-loading-screen PRIVATE ${DETOURS_LIBRARY} d3d9 wininet)

set_target_properties(custom-loading-screen PROPERTIES SUFFIX ".asi")

target_compile_definitions(custom-loading-screen PRIVATE WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)

# generate config.cfg
set(CONFIG_CONTENT "IMAGE_URL=YOUR_URL_IMAGE_HERE")
set(GENERATED_CONFIG "${CMAKE_BINARY_DIR}/generated_config.cfg")
file(WRITE "${GENERATED_CONFIG}" "${CONFIG_CONTENT}")

# copy config.cfg to plugin dir
add_custom_command(TARGET custom-loading-screen POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${GENERATED_CONFIG}"
    "$<TARGET_FILE_DIR:custom-loading-screen>/loadscs_config.cfg"
)



