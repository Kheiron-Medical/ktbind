cmake_minimum_required(VERSION 3.10)

# set the project name
project(ktbind LANGUAGES CXX)

# Java integration
find_package(JNI REQUIRED)

# compiler configuration
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS -Wfatal-errors)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

# sources
set(header_files ${CMAKE_CURRENT_SOURCE_DIR}/include/ktbind/ktbind.hpp)

# header-only target for Java/Kotlin integration
add_library(ktbind INTERFACE)
target_sources(ktbind INTERFACE "$<BUILD_INTERFACE:${header_files}>")
target_include_directories(ktbind INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/>)

# shared library for unit tests
add_library(ktbind_java MODULE test/java.cpp)
add_dependencies(ktbind_java ktbind)
target_include_directories(ktbind_java PRIVATE ${JNI_INCLUDE_DIRS})
target_link_libraries(ktbind_java PRIVATE ktbind ${JAVA_JVM_LIBRARY})

# installer
install(DIRECTORY include/ktbind DESTINATION include)
