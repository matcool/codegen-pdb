cmake_minimum_required(VERSION 3.20)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(LLVM REQUIRED CONFIG)

include(FetchContent)
FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz)
FetchContent_MakeAvailable(json)

if (WIN32)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif()

project(CodegenPDB)
add_executable(CodegenPDB
    src/main.cpp
    src/pdb_creator.cpp
    src/pe_file.cpp
)

target_include_directories(CodegenPDB PUBLIC ${LLVM_INCLUDE_DIRS})

llvm_map_components_to_libnames(llvm_libs core support DebugInfoPDB)
target_link_libraries(CodegenPDB ${llvm_libs} nlohmann_json::nlohmann_json)
