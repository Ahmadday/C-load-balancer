cmake_minimum_required(VERSION 3.10)
project(LoadBalancerSim CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()
set(CMAKE_CXX_FLAGS_RELEASE "-O2")

find_package(Threads REQUIRED)

include_directories(include)

add_executable(lbsim
    src/main.cpp
    src/Server.cpp
    src/LoadBalancer.cpp
    src/Strategies.cpp
    src/RequestGenerator.cpp
    src/Metrics.cpp
    src/LiveDashboard.cpp
)

target_link_libraries(lbsim PRIVATE Threads::Threads)
