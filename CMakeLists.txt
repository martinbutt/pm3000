cmake_minimum_required(VERSION 3.5.1)

project(pm3000)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/sdl2)

file(GLOB SOURCES "src/*.cpp" "src/screens/*.cpp")
add_executable(${PROJECT_NAME} ${SOURCES}
        external/nativefiledialog-extended/src/include/nfd.h
)

target_include_directories(${PROJECT_NAME} PRIVATE include src)

target_compile_options(${PROJECT_NAME} PRIVATE $<$<C_COMPILER_ID:MSVC>:/W4 /WX>)
target_compile_options(${PROJECT_NAME} PRIVATE $<$<NOT:$<C_COMPILER_ID:MSVC>>:-Wall -Wextra -pedantic>)

find_package(SDL2 REQUIRED)
target_link_libraries(${PROJECT_NAME} SDL2::Main)

find_package(SDL2_image REQUIRED)
target_link_libraries(${PROJECT_NAME} SDL2::Image)

find_package(SDL2_ttf REQUIRED)
target_link_libraries(${PROJECT_NAME} SDL2::TTF)

file(COPY assets DESTINATION ${CMAKE_BINARY_DIR})

# Add nfd
add_subdirectory(external/nativefiledialog-extended)
target_link_libraries(${PROJECT_NAME} nfd)

enable_testing()

add_executable(pm3_utils_tests tests/pm3_utils_tests.cpp)
target_include_directories(pm3_utils_tests PRIVATE src include)
target_sources(pm3_utils_tests PRIVATE
        src/pm3_data.cpp
        src/game_utils.cpp
        src/io.cpp
        src/input.cpp
        src/gfx.cpp)
target_link_libraries(pm3_utils_tests SDL2::Main SDL2::Image SDL2::TTF nfd)
add_test(NAME pm3_utils_tests COMMAND pm3_utils_tests)

add_executable(test_pm3_data tests/test_pm3_data.cpp)
target_include_directories(test_pm3_data PRIVATE src include)
target_sources(test_pm3_data PRIVATE src/pm3_data.cpp)
add_test(NAME test_pm3_data COMMAND test_pm3_data)

add_executable(test_io tests/test_io.cpp)
target_include_directories(test_io PRIVATE src include)
target_sources(test_io PRIVATE
        src/pm3_data.cpp
        src/io.cpp
        src/input.cpp
        src/gfx.cpp)
target_link_libraries(test_io SDL2::Main SDL2::Image SDL2::TTF nfd)
add_test(NAME test_io COMMAND test_io)

add_executable(test_game_utils tests/test_game_utils.cpp)
target_include_directories(test_game_utils PRIVATE src include)
target_sources(test_game_utils PRIVATE
        src/pm3_data.cpp
        src/game_utils.cpp
        src/io.cpp
        src/input.cpp
        src/gfx.cpp)
target_link_libraries(test_game_utils SDL2::Main SDL2::Image SDL2::TTF nfd)
add_test(NAME test_game_utils COMMAND test_game_utils)

add_executable(test_input tests/test_input.cpp)
target_include_directories(test_input PRIVATE src include)
target_sources(test_input PRIVATE
        src/pm3_data.cpp
        src/input.cpp
        src/gfx.cpp)
target_link_libraries(test_input SDL2::Main SDL2::Image SDL2::TTF nfd)
add_test(NAME test_input COMMAND test_input)

add_executable(test_text tests/test_text.cpp)
target_include_directories(test_text PRIVATE src include)
target_sources(test_text PRIVATE
        src/text.cpp
        src/pm3_data.cpp
        src/game_utils.cpp
        src/io.cpp
        src/input.cpp
        src/gfx.cpp)
target_link_libraries(test_text SDL2::Main SDL2::Image SDL2::TTF nfd)
add_test(NAME test_text COMMAND test_text)

add_executable(test_ui tests/test_ui.cpp)
target_include_directories(test_ui PRIVATE src include)
target_sources(test_ui PRIVATE
        src/ui.cpp
        src/text.cpp
        src/pm3_data.cpp
        src/game_utils.cpp
        src/io.cpp
        src/input.cpp
        src/gfx.cpp)
target_link_libraries(test_ui SDL2::Main SDL2::Image SDL2::TTF nfd)
add_test(NAME test_ui COMMAND test_ui)

add_executable(swos_import_tool tools/swos_import_tool.cpp)
target_include_directories(swos_import_tool PRIVATE src include)
target_sources(swos_import_tool PRIVATE
        src/swos_import.cpp
        src/swos_extract.cpp
        src/rnc_decompress.cpp
        src/io.cpp
        src/pm3_data.cpp
        src/game_utils.cpp
        src/input.cpp
        src/gfx.cpp)
target_link_libraries(swos_import_tool SDL2::Main SDL2::Image SDL2::TTF nfd)

add_executable(inspect_pm3_data tools/inspect_pm3_data.cpp)
target_include_directories(inspect_pm3_data PRIVATE src include)
target_sources(inspect_pm3_data PRIVATE
        src/pm3_data.cpp)
target_link_libraries(inspect_pm3_data SDL2::Main SDL2::Image SDL2::TTF nfd)