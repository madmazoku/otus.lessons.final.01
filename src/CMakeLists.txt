# global
cmake_minimum_required(VERSION 3.2)

if(DEFINED ENV{TRAVIS_BUILD_NUMBER})
    project(rq VERSION 0.0.$ENV{TRAVIS_BUILD_NUMBER})
else()
    project(rq VERSION 0.0.1)
    set(CMAKE_BUILD_TYPE Debug)
endif()

set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME ON)

find_package(Boost COMPONENTS unit_test_framework coroutine context thread filesystem system regex REQUIRED)
find_package(Threads REQUIRED)

set(CPACK_GENERATOR DEB)

set(CPACK_PACKAGE_VERSION_MAJOR "${PROJECT_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${PROJECT_VERSION_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${PROJECT_VERSION_PATCH}")

set(CPACK_PACKAGE_CONTACT madmazoku@gmail.com)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -pthread")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -std=c++11 -pthread")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -std=c++11 -pthread")
set(CMAKE_STATIC_LINKER_FLAGS "${CMAKE_STATIC_LINKER_FLAGS} -std=c++11 -pthread")

include(CPack)

enable_testing()

# local
configure_file(in.version.h version.h)

add_executable(rq_server server.cpp)
add_executable(rq_test test.cpp)

add_definitions(-DBOOST_COROUTINES_NO_DEPRECATION_WARNING)
add_definitions(-DBOOST_COROUTINE_NO_DEPRECATION_WARNING)

set_target_properties(rq_server rq_test PROPERTIES
    CXX_STANDARD 14
    CXX_STANDARD_REQUIRED ON
    COMPILE_OPTIONS -Wpedantic -Wall -Wextra
)

set_target_properties(rq_server rq_test PROPERTIES
    COMPILE_DEFINITIONS BOOST_TEST_STATIC_LINK
    INCLUDE_DIRECTORIES ${Boost_INCLUDE_DIR}
)

target_link_libraries(rq_server
    ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT}
)

target_link_libraries(rq_test
    ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT}
)

install(TARGETS rq_server rq_server
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
)
    
add_test(rq_server rq_test)
