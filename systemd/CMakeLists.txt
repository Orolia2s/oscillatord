pkg_get_variable(SYSTEMD_UNIT_DIR systemd systemdsystemunitdir)

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/oscillatord.service
        DESTINATION ${SYSTEMD_UNIT_DIR})
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/oscillatord@.service
        DESTINATION ${SYSTEMD_UNIT_DIR})
