add_library(shuttle_asset_core STATIC)

target_sources(
    shuttle_asset_core
    PRIVATE
        src/string_table.cpp
        src/container_table.cpp
        src/blob_reader.cpp
        src/blob_writer.cpp
        src/blob_view.cpp)

target_include_directories(
    shuttle_asset_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${PROJECT_SOURCE_DIR}/external/mio/single_include)

target_compile_features(shuttle_asset_core PUBLIC cxx_std_20)
