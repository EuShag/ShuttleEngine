#pragma once

// Решаем, какой бэкенд использовать
#if defined(_WIN32) && !defined(SHUTTLE_FORCE_SDL)
    #define SHUTTLE_PLATFORM_WIN32
    #include "PAL/Win32/Win32Platform.hpp"
    namespace shuttle::pal {
        using Platform = impl::Win32Platform;
    }
#else
    #define SHUTTLE_PLATFORM_SDL
    #include "PAL/Sdl/SdlPlatform.hpp"
    namespace shuttle::pal {
        using Platform = impl::SdlPlatform;
    }
#endif
