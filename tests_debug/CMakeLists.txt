if(BUILD_TESTS)

	file(GLOB_RECURSE SOURCES ${PROJECT_SOURCE_DIR}/src/*.[ch])
	file(GLOB COMMON_SOURCES ${PROJECT_SOURCE_DIR}/common/*.[ch])
	add_executable(gnss_start_stop_test ${SOURCES} ${COMMON_SOURCES})

	target_link_libraries(gnss_start_stop_test PRIVATE
	${oscillator-disciplining_LIBRARIES}
	${gps_LIBRARIES}
	${ubloxcfg_LIBRARIES}
	pthread
	m
	json-c)

	install(TARGETS gnss_start_stop_test RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

endif(BUILD_TESTS)