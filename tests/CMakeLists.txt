file(GLOB SIM_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/*.[ch])
add_executable(oscillator_sim ${SIM_SOURCES})
install(TARGETS oscillator_sim RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

add_subdirectory(tsync_stub)
