add_library(cmft STATIC
        ${CMAKE_SOURCE_DIR}/external/cmft/src/cmft/allocator.cpp
        ${CMAKE_SOURCE_DIR}/external/cmft/src/cmft/image.cpp
        ${CMAKE_SOURCE_DIR}/external/cmft/src/cmft/cubemapfilter.cpp
        ${CMAKE_SOURCE_DIR}/external/cmft/src/cmft/clcontext.cpp

        ${CMAKE_SOURCE_DIR}/external/cmft/src/cmft/common/print.cpp
        ${CMAKE_SOURCE_DIR}/external/cmft/src/cmft/common/stb_image.cpp
)

target_include_directories(cmft
        PUBLIC
        ${CMAKE_SOURCE_DIR}/external/cmft/include
        ${CMAKE_SOURCE_DIR}/external/cmft/src
        ${CMAKE_SOURCE_DIR}/external/cmft/dependency
)
