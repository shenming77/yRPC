message("* * * * * * * * * *  yRPC * * * * * * * * * * * * * * *")

cmake_minimum_required(VERSION 3.1.0)

project(yRPC LANGUAGES CXX)

# 默认使用C++14版本，推荐使用C++17版本。不支持C++11以下版本
set(CMAKE_CXX_STANDARD 17)
SET(CMAKE_BUILD_TYPE "Debug")
SET(CMAKE_CXX_FLAGS_RELEASE "-O2 -Wall -fPIC")
SET(CMAKE_CXX_FLAGS_DEBUG "-O0 -ggdb")

# 定义一个宏来查找多个必需的包
macro(find_required_packages)
  foreach(package ${ARGN})
    find_package(${package} REQUIRED)
  endforeach()
endmacro()

# 使用宏来添加依赖
find_required_packages(Threads yaml-cpp Protobuf)


include_directories (
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${YAML_CPP_INCLUDE_DIRS}
)

link_directories(
)



file(GLOB_RECURSE LibSource src/*.cpp protobuf/*.cc)
add_library(utils ${LibSource})
target_link_libraries(utils Threads::Threads zookeeper_mt ${YAML_CPP_LIBRARIES} )
# add_dependencies(utils)

add_executable(client app/userclient.cpp)
target_link_libraries(client protobuf utils)
add_dependencies(client utils)

add_executable(service app/userservice.cpp)
target_link_libraries(service protobuf utils)
add_dependencies(service utils)

