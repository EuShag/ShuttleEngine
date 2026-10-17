if(NOT WIN32)
    message(FATAL_ERROR "The PAL module currently supports Win32 only; add a Wayland or XCB backend before building on Linux.")
endif()

add_library(pal STATIC
    src/common/window/window_base.cpp
)

add_library(pal::pal ALIAS pal)

target_compile_features(pal PUBLIC cxx_std_23)

target_include_directories(pal
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${PROJECT_SOURCE_DIR}/external/imgui
        ${Vulkan_INCLUDE_DIRS}
)

target_sources(pal PRIVATE
    src/win32/win32_platform.cpp
    ${PROJECT_SOURCE_DIR}/external/imgui/backends/imgui_impl_win32.cpp
)

target_link_libraries(pal PUBLIC
    user32
    gdi32
    shell32
    shcore
    ole32
    dwmapi
    comctl32
)
