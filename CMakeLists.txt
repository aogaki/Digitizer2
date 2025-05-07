# ----------------------------------------------------------------------------
# Setup the project
cmake_minimum_required(VERSION 3.5 FATAL_ERROR)
project(dig2-test)
set(LIB_NAME Dig2)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 17)

# ---Locate the ROOT package and defines a number of variables (e.g. ROOT_INCLUDE_DIRS)
list(APPEND CMAKE_PREFIX_PATH $ENV{ROOTSYS})

# set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "$ENV{ROOTSYS}/ect/cmake")
find_package(ROOT REQUIRED COMPONENTS RIO Net)
include(${ROOT_USE_FILE})

set(CMAKE_CXX_FLAGS_DEBUG_INIT "-Wall")
set(CMAKE_CXX_FLAGS_RELEASE_INIT "-Wall")

add_compile_options(-pthread -O2 -fopenmp)

# ----------------------------------------------------------------------------
include_directories(${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${ROOT_INCLUDE_DIR})
link_directories(${ROOT_LIBRARY_DIR})
file(GLOB headers ${PROJECT_SOURCE_DIR}/include/*.hpp)
file(GLOB sources ${PROJECT_SOURCE_DIR}/src/*.cpp)

# Copy config files *.conf to build directory
file(GLOB conf_files ${PROJECT_SOURCE_DIR}/*.json ${PROJECT_SOURCE_DIR}/*.conf)
file(COPY ${conf_files} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

# ----------------------------------------------------------------------------
add_library(${LIB_NAME} SHARED ${sources} ${headers})
target_link_libraries(${LIB_NAME} ${ROOT_LIBRARIES} RHTTP gomp CAEN_FELib)
add_executable(${PROJECT_NAME} main.cpp)
target_link_libraries(${PROJECT_NAME} ${LIB_NAME})
