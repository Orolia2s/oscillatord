file(GLOB SIM_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/*.[ch])
add_executable(oscillator_sim ${SIM_SOURCES} ${PROJECT_SOURCE_DIR}/src/log.c ${PROJECT_SOURCE_DIR}/src/config.c ${PROJECT_SOURCE_DIR}/src/utils.c)
install(TARGETS oscillator_sim RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

add_subdirectory(lib_osc_sim_stubs)
