if(BUILD_TESTS)
	file(GLOB COMMON_SOURCES
		${PROJECT_SOURCE_DIR}/common/config.[ch]
		${PROJECT_SOURCE_DIR}/common/log.[ch]
		${PROJECT_SOURCE_DIR}/common/mro50_ioctl.h
		${PROJECT_SOURCE_DIR}/common/utils.[ch]
	)
	file(GLOB COMMON_GNSS_SOURCES
		${PROJECT_SOURCE_DIR}/common/f9_defvalsets.[ch]
		${PROJECT_SOURCE_DIR}/common/gnss-config.[ch]
	)
	file(GLOB mro_test_prod_SOURCES
		${PROJECT_SOURCE_DIR}/common/f9_defvalsets.[ch]
		${PROJECT_SOURCE_DIR}/common/gnss-config.[ch]
		${CMAKE_CURRENT_SOURCE_DIR}/mro_test_prod.c
		${PROJECT_SOURCE_DIR}/src/ntpshm/ppsthread.[ch]
		${PROJECT_SOURCE_DIR}/src/ntpshm/ntpshmread.[ch]
		${PROJECT_SOURCE_DIR}/src/ntpshm/timehint.[ch]
		${PROJECT_SOURCE_DIR}/src/ntpshm/timespec.[ch]
		${PROJECT_SOURCE_DIR}/src/ntpshm/timespec_str.[ch]
		${PROJECT_SOURCE_DIR}/src/ntpshm/ntpshm.[ch]
		${PROJECT_SOURCE_DIR}/src/ntpshm/ntpshmread.[ch]
		${PROJECT_SOURCE_DIR}/src/ntpshm/ntpshmwrite.[ch]
		${PROJECT_SOURCE_DIR}/src/phasemeter.[ch]
		${PROJECT_SOURCE_DIR}/src/gnss.[ch]
		${PROJECT_SOURCE_DIR}/src/oscillator.[ch]
		${PROJECT_SOURCE_DIR}/src/oscillator_factory.[ch]
		${PROJECT_SOURCE_DIR}/src/oscillators/mRo50_oscillator.c
	)

	pkg_check_modules(SYSTEMD REQUIRED libsystemd)
	include_directories(${SYSTEMD_INCLUDE_DIRS})

	add_executable(mro_test_prod ${mro_test_prod_SOURCES} ${COMMON_SOURCES})

	target_link_libraries(mro_test_prod PRIVATE m pthread ${ubloxcfg_LIBRARIES})

	install(TARGETS mro_test_prod RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

endif(BUILD_TESTS)
