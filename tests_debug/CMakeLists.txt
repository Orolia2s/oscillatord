if(BUILD_TESTS)

	file(GLOB_RECURSE SOURCES ${PROJECT_SOURCE_DIR}/src/*.[ch])
	file(GLOB COMMON_SOURCES ${PROJECT_SOURCE_DIR}/common/*.[ch])


	file(GLOB gnss_start_stop_test_SOURCES
		${PROJECT_SOURCE_DIR}/common/f9_defvalsets.[ch]
		${PROJECT_SOURCE_DIR}/src/gnss.[ch]
		${PROJECT_SOURCE_DIR}/common/gnss-config.[ch]
		${CMAKE_CURRENT_SOURCE_DIR}/gnss_start_stop_test.c
		${PROJECT_SOURCE_DIR}/src/ntpshm/ppsthread.[ch]
		${PROJECT_SOURCE_DIR}/src/ntpshm/ntpshmread.[ch]
		${PROJECT_SOURCE_DIR}/src/ntpshm/timehint.[ch]
		${PROJECT_SOURCE_DIR}/src/ntpshm/timespec.[ch]
		${PROJECT_SOURCE_DIR}/src/ntpshm/timespec_str.[ch]
		${PROJECT_SOURCE_DIR}/src/ntpshm/ntpshm.[ch]
		${PROJECT_SOURCE_DIR}/src/ntpshm/ntpshmread.[ch]
		${PROJECT_SOURCE_DIR}/src/ntpshm/ntpshmwrite.[ch]
	)

	add_executable(gnss_start_stop_test ${gnss_start_stop_test_SOURCES} ${COMMON_SOURCES})

	target_link_libraries(gnss_start_stop_test PRIVATE
	${oscillator-disciplining_LIBRARIES}
	${gps_LIBRARIES}
	${ubloxcfg_LIBRARIES}
	pthread
	m
	json-c)

	install(TARGETS gnss_start_stop_test RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

endif(BUILD_TESTS)