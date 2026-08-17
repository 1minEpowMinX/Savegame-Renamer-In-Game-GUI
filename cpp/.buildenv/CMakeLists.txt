project(SavegameRenamer LANGUAGES CXX)

find_package(minhook CONFIG REQUIRED)

file(GLOB_RECURSE PLUGIN_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/../src/*.cpp")

add_library(SavegameRenamer SHARED ${PLUGIN_SOURCES})

target_precompile_headers(SavegameRenamer PRIVATE "${RE_BUILDENV}/PCH.h")

target_compile_definitions(SavegameRenamer PRIVATE
    _ITERATOR_DEBUG_LEVEL=0 WIN32_LEAN_AND_MEAN NOMINMAX)

target_compile_options(SavegameRenamer PRIVATE /utf-8 /W4 /wd4819 /wd4100)

target_include_directories(SavegameRenamer PRIVATE
    ${RE_ROOT}/include ${RE_ROOT}/include/CryEngine/CryCommon
    ${CMAKE_CURRENT_SOURCE_DIR}/../include ${CMAKE_CURRENT_SOURCE_DIR}/../src)

target_link_libraries(SavegameRenamer PRIVATE kcd_re minhook::minhook)

# The whs:: model depends on neither the game nor kcd_re, so its tests build
# against the sources directly: no PCH, no libKCD2 include paths, no game headers.
find_package(Catch2 CONFIG REQUIRED)
include(CTest)
include(Catch)

file(GLOB TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/../tests/*.cpp")
file(GLOB MODEL_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/../src/whs/*.cpp")

if(TEST_SOURCES)
    add_executable(SavegameRenamerTests ${TEST_SOURCES} ${MODEL_SOURCES})

    target_include_directories(SavegameRenamerTests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
        ${CMAKE_CURRENT_SOURCE_DIR}/../tests)

    target_compile_options(SavegameRenamerTests PRIVATE /utf-8 /W4)

    target_link_libraries(SavegameRenamerTests PRIVATE Catch2::Catch2WithMain)

    catch_discover_tests(SavegameRenamerTests)
endif()
