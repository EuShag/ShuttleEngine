add_library(ShuttleAssetCore STATIC
        src/StringTable.cpp
        src/ContainerTable.cpp
        src/BlobReader.cpp
        src/BlobWriter.cpp
        src/BlobView.cpp
)

target_include_directories(ShuttleAssetCore
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        PRIVATE
        ${PROJECT_SOURCE_DIR}/external/mio/single_include
)

target_compile_features(ShuttleAssetCore
        PUBLIC
        cxx_std_20
)