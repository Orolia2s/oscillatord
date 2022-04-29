if(BUILD_UTILS)
	file(GLOB ART_EEPROM_MANAGER_SOURCES
		${CMAKE_CURRENT_SOURCE_DIR}/art_disciplining_manager.c
	)
	file(GLOB COMMON_SOURCES
		${PROJECT_SOURCE_DIR}/common/config.[ch]
		${PROJECT_SOURCE_DIR}/common/log.[ch]
		${PROJECT_SOURCE_DIR}/common/mro50_ioctl.h
		${PROJECT_SOURCE_DIR}/common/utils.[ch]
	)
	file(GLOB EEPROM_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/eeprom.[ch])
	file(GLOB EXTTS_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/extts.[ch])
	file(GLOB ART_EEPROM_FORMAT_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/art_eeprom_format.c)
	file(GLOB ART_EEPROM_DISPLAY_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/art_eeprom_display.c)

	add_executable(art_disciplining_manager ${ART_EEPROM_MANAGER_SOURCES} ${COMMON_SOURCES} ${EEPROM_SOURCES})
	add_executable(art_eeprom_format ${ART_EEPROM_FORMAT_SOURCES} ${COMMON_SOURCES} ${EEPROM_SOURCES})
	add_executable(art_eeprom_display ${ART_EEPROM_DISPLAY_SOURCES} ${COMMON_SOURCES} ${EEPROM_SOURCES})

	target_link_libraries(art_disciplining_manager PRIVATE
		m)
	target_link_libraries(art_eeprom_format PRIVATE
		${oscillator-disciplining_LIBRARIES}
		m)
	target_link_libraries(art_eeprom_display PRIVATE
		m)

	install(TARGETS art_disciplining_manager RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
	install(TARGETS art_eeprom_format RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
	install(TARGETS art_eeprom_display RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

endif(BUILD_UTILS)
