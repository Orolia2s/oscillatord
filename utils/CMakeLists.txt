if(BUILD_UTILS)
	file(GLOB ART_EEPROM_MANAGER_SOURCES
		${CMAKE_CURRENT_SOURCE_DIR}/art_eeprom_manager.c
	)
	file(GLOB COMMON_SOURCES ${PROJECT_SOURCE_DIR}/common/*.[ch])

	add_executable(art_eeprom_manager ${ART_EEPROM_MANAGER_SOURCES} ${COMMON_SOURCES})
	target_link_libraries(art_eeprom_manager PRIVATE
		m)
	install(TARGETS art_eeprom_manager RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

endif(BUILD_UTILS)
