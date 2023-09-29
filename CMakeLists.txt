cmake_minimum_required(VERSION 3.12.4)
set(CMAKE_DISABLE_SOURCE_CHANGES ON)
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)
project(wsong LANGUAGES C CXX)

# Version
set(libwsong_VERSION 1.0rc0)
set(libwsong_build_VERSION 1.0rc0)

# C++ STANDARD
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_C_STANDARD 11)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(CMAKE_CXX_FLAGS_DEBUG             "${CMAKE_CXX_FLAGS_DEBUG} -O0 -Wall -ggdb -gdwarf-3 -ftemplate-backtrace-limit=0")
set(CMAKE_CXX_FLAGS_RELEASE           "${CMAKE_CXX_FLAGS_RELEASE} -O3 -mprefer-vector-width=512 -Wall -fcompare-debug-second")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO    "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -Wall -ggdb -dgwarf-3 -O3")

set(CMAKE_C_FLAGS_DEBUG             "${CMAKE_C_FLAGS_DEBUG} -O0 -Wall -ggdb -gdwarf-3 -ftemplate-backtrace-limit=0")
set(CMAKE_C_FLAGS_RELEASE           "${CMAKE_C_FLAGS_RELEASE} -O3 -mprefer-vector-width=512 -Wall -fcompare-debug-second")
set(CMAKE_C_FLAGS_RELWITHDEBINFO    "${CMAKE_C_FLAGS_RELWITHDEBINFO} -Wall -ggdb -dgwarf-3 -O3")

# find cache size
execute_process(COMMAND getconf LEVEL1_DCACHE_LINESIZE OUTPUT_VARIABLE CLSZ)
string(STRIP ${CLSZ} CLSZ)
set(CACHELINE_SIZE ${CLSZ})

include(GNUInstallDirs)

if ( NOT DEFINED CMAKE_INSTALL_LIBDIR )
    set( CMAKE_INSTALL_LIBDIR lib )
endif ()

if ( NOT DEFINED CMAKE_INSTALL_INCLUDEDIR )
    set( CMAKE_INSTALL_INCLUDEDIR include )
endif ( )

# doxygen
find_package(Doxygen)

CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/include/wsong/config.h)
add_subdirectory(src/perf)
add_subdirectory(src/applications)

add_library(wsong SHARED
    $<TARGET_OBJECTS:timing>
)

# make install
install(TARGETS wsong EXPORT wsongTargets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(DIRECTORY
        ${CMAKE_CURRENT_SOURCE_DIR}/include/wsong
        ${CMAKE_CURRENT_BINARY_DIR}/include/wsong
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        FILES_MATCHING
        PATTERN "*.h"
        PATTERN "*.hpp")
