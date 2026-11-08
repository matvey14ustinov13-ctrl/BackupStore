cmake_minimum_required(VERSION 3.20)
project(BackupStore VERSION 1.0.0 LANGUAGES C CXX RC)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_executable(BackupStore WIN32
    src/main.cpp
    src/common.cpp
    src/http.cpp
    src/catalog.cpp
    src/install.cpp
    src/render.cpp
    third_party/miniz.c
    third_party/miniz_tdef.c
    third_party/miniz_tinfl.c
    third_party/miniz_zip.c
    res/BackupStore.rc
)

target_include_directories(BackupStore PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/third_party")

if(MSVC)
    target_compile_definitions(BackupStore PRIVATE _WIN32_WINNT=0x0A00 WINVER=0x0A00)
    target_compile_options(BackupStore PRIVATE /utf-8 /W4 /permissive-)
    target_link_options(BackupStore PRIVATE /MANIFEST:NO)
    set(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} /I\"${CMAKE_CURRENT_SOURCE_DIR}/res\"")
endif()

target_link_libraries(BackupStore PRIVATE
    user32
    gdi32
    shell32
    advapi32
    comctl32
    winhttp
    shlwapi
    ole32
    oleaut32
    uuid
    dwmapi
)

set_target_properties(BackupStore PROPERTIES
    OUTPUT_NAME "BackupStore"
    WIN32_EXECUTABLE TRUE
)
