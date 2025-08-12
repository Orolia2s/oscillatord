pkg_get_variable(SYSTEMD_UNIT_DIR systemd systemdsystemunitdir)

configure_file(oscillatord.service.in ${CMAKE_CURRENT_BINARY_DIR}/oscillatord.service @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/oscillatord.service
        DESTINATION ${SYSTEMD_UNIT_DIR})
configure_file(oscillatord@.service.in ${CMAKE_CURRENT_BINARY_DIR}/oscillatord@.service @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/oscillatord@.service
        DESTINATION ${SYSTEMD_UNIT_DIR})
