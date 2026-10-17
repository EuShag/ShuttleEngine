# CMake generated Testfile for 
# Source directory: E:/Users/Shagu/Projects/ShuttleEngine/external/mimalloc
# Build directory: E:/Users/Shagu/Projects/ShuttleEngine/cmake-build-mingw-release/external/mimalloc
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(test-api "E:/Users/Shagu/Projects/ShuttleEngine/cmake-build-mingw-release/bin/mimalloc-test-api.exe")
set_tests_properties(test-api PROPERTIES  _BACKTRACE_TRIPLES "E:/Users/Shagu/Projects/ShuttleEngine/external/mimalloc/CMakeLists.txt;1167;add_test;E:/Users/Shagu/Projects/ShuttleEngine/external/mimalloc/CMakeLists.txt;0;")
add_test(test-api-fill "E:/Users/Shagu/Projects/ShuttleEngine/cmake-build-mingw-release/bin/mimalloc-test-api-fill.exe")
set_tests_properties(test-api-fill PROPERTIES  _BACKTRACE_TRIPLES "E:/Users/Shagu/Projects/ShuttleEngine/external/mimalloc/CMakeLists.txt;1167;add_test;E:/Users/Shagu/Projects/ShuttleEngine/external/mimalloc/CMakeLists.txt;0;")
add_test(test-stress-heaps "E:/Program Files/CMake/bin/cmake.exe" "-E" "env" "MIMALLOC_ARENA_EAGER_COMMIT=0" "MIMALLOC_PAGE_COMMIT_ON_DEMAND=0" "E:/Users/Shagu/Projects/ShuttleEngine/cmake-build-mingw-release/bin/mimalloc-test-stress-heaps.exe")
set_tests_properties(test-stress-heaps PROPERTIES  _BACKTRACE_TRIPLES "E:/Users/Shagu/Projects/ShuttleEngine/external/mimalloc/CMakeLists.txt;1163;add_test;E:/Users/Shagu/Projects/ShuttleEngine/external/mimalloc/CMakeLists.txt;0;")
add_test(test-stress-subprocs "E:/Users/Shagu/Projects/ShuttleEngine/cmake-build-mingw-release/bin/mimalloc-test-stress-subprocs.exe")
set_tests_properties(test-stress-subprocs PROPERTIES  _BACKTRACE_TRIPLES "E:/Users/Shagu/Projects/ShuttleEngine/external/mimalloc/CMakeLists.txt;1167;add_test;E:/Users/Shagu/Projects/ShuttleEngine/external/mimalloc/CMakeLists.txt;0;")
add_test(test-stress "E:/Users/Shagu/Projects/ShuttleEngine/cmake-build-mingw-release/bin/mimalloc-test-stress.exe")
set_tests_properties(test-stress PROPERTIES  _BACKTRACE_TRIPLES "E:/Users/Shagu/Projects/ShuttleEngine/external/mimalloc/CMakeLists.txt;1167;add_test;E:/Users/Shagu/Projects/ShuttleEngine/external/mimalloc/CMakeLists.txt;0;")
add_test(test-profile "E:/Users/Shagu/Projects/ShuttleEngine/cmake-build-mingw-release/bin/mimalloc-test-profile.exe")
set_tests_properties(test-profile PROPERTIES  _BACKTRACE_TRIPLES "E:/Users/Shagu/Projects/ShuttleEngine/external/mimalloc/CMakeLists.txt;1167;add_test;E:/Users/Shagu/Projects/ShuttleEngine/external/mimalloc/CMakeLists.txt;0;")
add_test(test-stress-static "E:/Program Files/CMake/bin/cmake.exe" "-E" "env" "MIMALLOC_VERBOSE=1" "MIMALLOC_SHOW_STATS=1" "E:/Users/Shagu/Projects/ShuttleEngine/cmake-build-mingw-release/bin/mimalloc-test-stress-static.exe")
set_tests_properties(test-stress-static PROPERTIES  _BACKTRACE_TRIPLES "E:/Users/Shagu/Projects/ShuttleEngine/external/mimalloc/CMakeLists.txt;1178;add_test;E:/Users/Shagu/Projects/ShuttleEngine/external/mimalloc/CMakeLists.txt;0;")
add_test(test-stress-dynamic "E:/Program Files/CMake/bin/cmake.exe" "-E" "env" "MIMALLOC_VERBOSE=1" "E:/Users/Shagu/Projects/ShuttleEngine/cmake-build-mingw-release/bin/mimalloc-test-stress-dynamic.exe")
set_tests_properties(test-stress-dynamic PROPERTIES  _BACKTRACE_TRIPLES "E:/Users/Shagu/Projects/ShuttleEngine/external/mimalloc/CMakeLists.txt;1195;add_test;E:/Users/Shagu/Projects/ShuttleEngine/external/mimalloc/CMakeLists.txt;0;")
