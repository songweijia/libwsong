cmake_minimum_required(VERSION 3.22.1)
set(CMAKE_DISABLE_SOURCE_CHANGES ON)
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
project(wsong LANGUAGES C CXX)

# Version
set(libwsong_VERSION 1.0rc0)
set(libwsong_build_VERSION 1.0rc0)


# C++ STANDARD
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_STANDARD 11)

# FLAGS
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

# find default page size
execute_process(COMMAND getconf PAGESIZE OUTPUT_VARIABLE PGSZ)
string(STRIP ${PGSZ} PGSZ)
set(PAGE_SIZE ${PGSZ})

# build dependences
if( NOT EXISTS ${CMAKE_BINARY_DIR}/deps)
execute_process(COMMAND ${CMAKE_SOURCE_DIR}/scripts/install-jemalloc.sh ${CMAKE_BINARY_DIR}/deps)
endif ()
include_directories(${CMAKE_BINARY_DIR}/deps/include)
link_directories(${CMAKE_BINARY_DIR}/deps/lib)

include(GNUInstallDirs)

if ( NOT DEFINED CMAKE_INSTALL_LIBDIR )
    set( CMAKE_INSTALL_LIBDIR lib )
endif ()

if ( NOT DEFINED CMAKE_INSTALL_INCLUDEDIR )
    set( CMAKE_INSTALL_INCLUDEDIR include )
endif ( )

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/Modules")

# doxygen
find_package(Doxygen)

CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/include/wsong/config.h)
add_subdirectory(src/perf)
add_subdirectory(src/ipc)
add_subdirectory(src/applications)

add_library(perf SHARED
    $<TARGET_OBJECTS:perf_objs>
)
set_target_properties(perf PROPERTIES
    OUTPUT_NAME wsongperf
)

add_library(ipc SHARED
    $<TARGET_OBJECTS:ipc_objs>
)
target_link_libraries(ipc PRIVATE jemalloc_pic)
set_target_properties(ipc PROPERTIES
    OUTPUT_NAME wsongipc
)

# make install
install(TARGETS perf ipc EXPORT libwsongTargets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(DIRECTORY
        ${CMAKE_CURRENT_SOURCE_DIR}/include/wsong
        ${CMAKE_CURRENT_BINARY_DIR}/include/wsong
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        FILES_MATCHING
        PATTERN "*.h"
        PATTERN "*.hpp")

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/cmake/libwsongConfigVersion.cmake"
    VERSION ${libwsong_VERSION}
    COMPATIBILITY AnyNewerVersion
)

export (EXPORT libwsongTargets
    FILE "${CMAKE_CURRENT_BINARY_DIR}/cmake/libwsongTargets.cmake"
)

set(ConfigPackageLocation ${CMAKE_INSTALL_LIBDIR}/cmake/libwsong)

configure_package_config_file(libwsongConfig.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/cmake/libwsongConfig.cmake"
    INSTALL_DESTINATION ${ConfigPackageLocation}
    PATH_VARS CMAKE_INSTALL_LIBDIR CMAKE_INSTALL_INCLUDEDIR
)

install(EXPORT libwsongTargets
    FILE libwsongTargets.cmake
    NAMESPACE libwsong::
    DESTINATION ${ConfigPackageLocation}
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/cmake/libwsongConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake/libwsongConfigVersion.cmake"
    DESTINATION ${ConfigPackageLocation}
)

# documentation
set (DOXYGEN_EXCLUDE src/applications)
set (DOXYGEN_PROJECT_NUMBER ${libwsong_VERSION})
set (DOXYGEN_TAB_SIZE 4)
set (DOXYGEN_WARNINGS NO)
set (DOXYGEN_WARN_IF_UNDOCUMENTED NO)
set (DOXYGEN_WARN_NO_PARAMDOC NO)
if (DOXYGEN_FOUND)
    # create docs
    doxygen_add_docs(docs
        ${CMAKE_SOURCE_DIR}/README.md
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ALL
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
endif()
