# CMake generated Testfile for 
# Source directory: E:/Users/Shagu/Projects/ShuttleEngine/async
# Build directory: E:/Users/Shagu/Projects/ShuttleEngine/cmake-build-async-test/async
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
if(CTEST_CONFIGURATION_TYPE MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
  add_test([=[async_one_thread_executor]=] "E:/Users/Shagu/Projects/ShuttleEngine/cmake-build-async-test/bin/Debug/async_one_thread_executor_test.exe")
  set_tests_properties([=[async_one_thread_executor]=] PROPERTIES  _BACKTRACE_TRIPLES "E:/Users/Shagu/Projects/ShuttleEngine/async/CMakeLists.txt;35;add_test;E:/Users/Shagu/Projects/ShuttleEngine/async/CMakeLists.txt;0;")
elseif(CTEST_CONFIGURATION_TYPE MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
  add_test([=[async_one_thread_executor]=] "E:/Users/Shagu/Projects/ShuttleEngine/cmake-build-async-test/bin/Release/async_one_thread_executor_test.exe")
  set_tests_properties([=[async_one_thread_executor]=] PROPERTIES  _BACKTRACE_TRIPLES "E:/Users/Shagu/Projects/ShuttleEngine/async/CMakeLists.txt;35;add_test;E:/Users/Shagu/Projects/ShuttleEngine/async/CMakeLists.txt;0;")
elseif(CTEST_CONFIGURATION_TYPE MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
  add_test([=[async_one_thread_executor]=] "E:/Users/Shagu/Projects/ShuttleEngine/cmake-build-async-test/bin/MinSizeRel/async_one_thread_executor_test.exe")
  set_tests_properties([=[async_one_thread_executor]=] PROPERTIES  _BACKTRACE_TRIPLES "E:/Users/Shagu/Projects/ShuttleEngine/async/CMakeLists.txt;35;add_test;E:/Users/Shagu/Projects/ShuttleEngine/async/CMakeLists.txt;0;")
elseif(CTEST_CONFIGURATION_TYPE MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
  add_test([=[async_one_thread_executor]=] "E:/Users/Shagu/Projects/ShuttleEngine/cmake-build-async-test/bin/RelWithDebInfo/async_one_thread_executor_test.exe")
  set_tests_properties([=[async_one_thread_executor]=] PROPERTIES  _BACKTRACE_TRIPLES "E:/Users/Shagu/Projects/ShuttleEngine/async/CMakeLists.txt;35;add_test;E:/Users/Shagu/Projects/ShuttleEngine/async/CMakeLists.txt;0;")
else()
  add_test([=[async_one_thread_executor]=] NOT_AVAILABLE)
endif()
