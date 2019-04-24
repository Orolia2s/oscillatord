file(GLOB SIM_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/*.[ch])
file(GLOB COMMON_SOURCES ${PROJECT_SOURCE_DIR}/common/*.[ch])
add_executable(oscillator_sim ${SIM_SOURCES} ${COMMON_SOURCES})
install(TARGETS oscillator_sim RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

add_subdirectory(lib_osc_sim_stubs)
