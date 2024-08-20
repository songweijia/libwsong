cmake_minimum_required(VERSION 3.22.1)
set(CMAKE_DISABLE_SOURCE_CHANGES ON)
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)

set(IPC_SOURCES ring_buffer.cpp shmpool.cpp buddy_system.cpp)
add_library(ipc_objs OBJECT
    ${IPC_SOURCES})
target_include_directories(ipc_objs PRIVATE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)

add_executable(ipc_cli ipc_cli.cpp)
target_include_directories(ipc_cli PRIVATE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)
target_link_libraries(ipc_cli ipc_objs jemalloc_pic)

install(TARGETS ipc_cli
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(CODE "execute_process( \
    COMMAND ${CMAKE_COMMAND} -E create_symlink \
    ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}/ipc_cli \
    ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}/rb_cli \
    )"
)
install(CODE "execute_process( \
    COMMAND ${CMAKE_COMMAND} -E create_symlink \
    ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}/ipc_cli \
    ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}/shmp_cli \
    )"
)
