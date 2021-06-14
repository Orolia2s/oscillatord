file(GLOB SIM_SOURCES
	${CMAKE_CURRENT_SOURCE_DIR}/oscillator_sim.c
	${CMAKE_CURRENT_SOURCE_DIR}/ptspair.c
	${CMAKE_CURRENT_SOURCE_DIR}/ptspair.h
)
file(GLOB COMMON_SOURCES ${PROJECT_SOURCE_DIR}/common/*.[ch])
file(GLOB MONITORING_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/monitoring_client.c)
add_executable(oscillator_sim ${SIM_SOURCES} ${COMMON_SOURCES})
add_executable(monitoring ${MONITORING_SOURCES} ${COMMON_SOURCES})
target_link_libraries(monitoring PRIVATE
	json-c)

install(TARGETS oscillator_sim RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(TARGETS monitoring RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

add_subdirectory(lib_osc_sim_stubs)
