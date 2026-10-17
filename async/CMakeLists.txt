add_library(async STATIC
    src/async_task.cpp
    src/awaiters.cpp
    src/coro_scope.cpp
    src/mi_allocator.cpp
    src/one_thread_executor.cpp
    src/thread_pool_executor/thread_pool_dispatcher_task.cpp
    src/thread_pool_executor/thread_pool_executor.cpp
)

add_library(async::async ALIAS async)

target_compile_features(async PUBLIC cxx_std_23)

target_include_directories(async
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/external/concurrentqueue
    PRIVATE
        ${PROJECT_SOURCE_DIR}/external/mimalloc/include
)

target_link_libraries(async PUBLIC mimalloc-static)

option(ASYNC_BUILD_TESTS "Build async module tests" OFF)

if(ASYNC_BUILD_TESTS)
    enable_testing()

    add_executable(async_one_thread_executor_test
        tests/one_thread_executor_test.cpp
    )
    target_link_libraries(async_one_thread_executor_test PRIVATE async::async)
    add_test(NAME async_one_thread_executor COMMAND async_one_thread_executor_test)
    set_tests_properties(async_one_thread_executor PROPERTIES TIMEOUT 10)
endif()
