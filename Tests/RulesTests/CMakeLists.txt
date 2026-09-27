# DARK BLOOD - standalone build + unit tests for the engine-independent rules core.
# The same sources are compiled by Unreal Build Tool as the DarkBloodRules module.
cmake_minimum_required(VERSION 3.20)
project(DarkBloodRulesTests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(RULES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../Source/DarkBloodRules)

file(GLOB RULES_SOURCES CONFIGURE_DEPENDS ${RULES_DIR}/Private/*.cpp)
# The module entry point is the only file that includes Unreal headers.
list(FILTER RULES_SOURCES EXCLUDE REGEX ".*DarkBloodRulesModule\\.cpp$")

add_library(DarkBloodRules STATIC ${RULES_SOURCES})
target_include_directories(DarkBloodRules PUBLIC ${RULES_DIR}/Public)

# Mirror Unreal's defaults so code that builds here also builds in UBT.
if(MSVC)
  target_compile_options(DarkBloodRules PUBLIC /W4 /WX /utf-8 /permissive- /GR- /EHs-c- /D_HAS_EXCEPTIONS=0)
else()
  target_compile_options(DarkBloodRules PUBLIC -Wall -Wextra -Wpedantic -Wshadow -Werror -fno-exceptions -fno-rtti)
endif()

option(DB_SANITIZE "Build with address/undefined sanitizers" OFF)
if(DB_SANITIZE AND NOT MSVC)
  target_compile_options(DarkBloodRules PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_link_options(DarkBloodRules PUBLIC -fsanitize=address,undefined)
endif()

file(GLOB TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
add_executable(DarkBloodRulesTests ${TEST_SOURCES})
target_link_libraries(DarkBloodRulesTests PRIVATE DarkBloodRules)

enable_testing()
add_test(NAME DarkBloodRulesTests COMMAND DarkBloodRulesTests)
