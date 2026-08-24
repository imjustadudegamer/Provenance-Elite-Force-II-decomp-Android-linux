    # Configure the .desktop entries with the arch suffix
    configure_file(
    misc/linux/org.openmoh.provenanceef2.desktop.in
    ${CMAKE_BINARY_DIR}/misc/linux/org.openmoh.provenanceef2.desktop
    @ONLY
    )
    configure_file(
    misc/linux/org.openmoh.provenanceef2b.desktop.in
    ${CMAKE_BINARY_DIR}/misc/linux/org.openmoh.provenanceef2b.desktop
    @ONLY
    )
    configure_file(
    misc/linux/org.openmoh.provenanceef2s.desktop.in
    ${CMAKE_BINARY_DIR}/misc/linux/org.openmoh.provenanceef2s.desktop
    @ONLY
    )

    # Install .desktop entries
    install(FILES ${CMAKE_BINARY_DIR}/misc/linux/org.openmoh.provenanceef2.desktop  DESTINATION ${INSTALL_DATADIR_FULL}/applications)
    install(FILES ${CMAKE_BINARY_DIR}/misc/linux/org.openmoh.provenanceef2b.desktop DESTINATION ${INSTALL_DATADIR_FULL}/applications)
    install(FILES ${CMAKE_BINARY_DIR}/misc/linux/org.openmoh.provenanceef2s.desktop DESTINATION ${INSTALL_DATADIR_FULL}/applications)

    install(FILES misc/linux/org.openmoh.provenanceef2.metainfo.xml DESTINATION ${INSTALL_DATADIR_FULL}/metainfo)

    install(FILES misc/provenanceef2.svg DESTINATION ${INSTALL_DATADIR_FULL}/icons/hicolor/symbolic/apps/ RENAME org.openmoh.provenanceef2.svg)
