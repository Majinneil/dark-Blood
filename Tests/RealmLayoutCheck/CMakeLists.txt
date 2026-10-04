# DARK BLOOD - engine-free check of the realm layout (rivers and terrain), see main.cpp.
#   cmake -S Tests/RealmLayoutCheck -B build/realm && cmake --build build/realm && build/realm/RealmLayoutCheck
cmake_minimum_required(VERSION 3.20)
project(DarkBloodRealmLayoutCheck LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(GAME_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../Source/DarkBlood)
add_executable(RealmLayoutCheck main.cpp ${GAME_DIR}/Private/World/DBRealmLayout.cpp)
# CoreMinimal.h here stands in for the engine's.
target_include_directories(RealmLayoutCheck PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${GAME_DIR}/Public)
if(MSVC)
  target_compile_options(RealmLayoutCheck PRIVATE /W4 /utf-8)
else()
  target_compile_options(RealmLayoutCheck PRIVATE -Wall -Wextra -Wshadow)
endif()
