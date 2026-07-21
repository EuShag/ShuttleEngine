# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file LICENSE.rst or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "E:/Users/Shagu/Projects/ShuttleEngine/external/compressonator/../common/lib/ext/ktx")
  file(MAKE_DIRECTORY "E:/Users/Shagu/Projects/ShuttleEngine/external/compressonator/../common/lib/ext/ktx")
endif()
file(MAKE_DIRECTORY
  "E:/Users/Shagu/Projects/ShuttleEngine/external/compressonator/../common/lib/ext/ktx/build"
  "E:/Users/Shagu/Projects/ShuttleEngine/external/compressonator/../common/lib/ext/ktx"
  "E:/Users/Shagu/Projects/ShuttleEngine/external/compressonator/../common/lib/ext/ktx/tmp"
  "E:/Users/Shagu/Projects/ShuttleEngine/external/compressonator/../common/lib/ext/ktx/src/extern_ktx-stamp"
  "E:/Users/Shagu/Projects/ShuttleEngine/cmake-build-debug/external/compressonator"
  "E:/Users/Shagu/Projects/ShuttleEngine/external/compressonator/../common/lib/ext/ktx/src/extern_ktx-stamp"
)

set(configSubDirs Debug;Release)
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "E:/Users/Shagu/Projects/ShuttleEngine/external/compressonator/../common/lib/ext/ktx/src/extern_ktx-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "E:/Users/Shagu/Projects/ShuttleEngine/external/compressonator/../common/lib/ext/ktx/src/extern_ktx-stamp${cfgdir}") # cfgdir has leading slash
endif()
