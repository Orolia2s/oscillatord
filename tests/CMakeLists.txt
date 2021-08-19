file(GLOB SIM_SOURCES
	${CMAKE_CURRENT_SOURCE_DIR}/oscillator_sim.c
	${CMAKE_CURRENT_SOURCE_DIR}/ptspair.c
	${CMAKE_CURRENT_SOURCE_DIR}/ptspair.h
)
file(GLOB COMMON_SOURCES ${PROJECT_SOURCE_DIR}/common/*.[ch])
file(GLOB MONITORING_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/monitoring_client.c)
file(GLOB MRO50_CTRL_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/mro50_ctrl.c)
file(GLOB ART_INTEGRATION_TEST_SUITE_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/art_integration_test_suite.c)
add_executable(oscillator_sim ${SIM_SOURCES} ${COMMON_SOURCES})
add_executable(monitoring ${MONITORING_SOURCES} ${COMMON_SOURCES})
add_executable(mro50_ctrl ${MRO50_CTRL_SOURCES} ${COMMON_SOURCES})
add_executable(art_integration_test_suite ${ART_INTEGRATION_TEST_SUITE_SOURCES} ${COMMON_SOURCES})
target_link_libraries(oscillator_sim PRIVATE
	m)
target_link_libraries(monitoring PRIVATE
	json-c
	m)
target_link_libraries(mro50_ctrl PRIVATE
	m)
target_link_libraries(art_integration_test_suite PRIVATE
	m
	${ubloxcfg_LIBRARIES})

install(TARGETS oscillator_sim RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(TARGETS monitoring RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(TARGETS mro50_ctrl RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(TARGETS art_integration_test_suite RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

add_subdirectory(lib_osc_sim_stubs)
