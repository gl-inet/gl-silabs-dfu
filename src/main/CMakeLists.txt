set( EXEC_NAME "gl-silabs-dfu" )

aux_source_directory(./ SOURCES)
aux_source_directory(../bgapi SOURCES)
aux_source_directory(../xmodem SOURCES)
aux_source_directory(../uart-driver SOURCES)


include_directories(./ )
include_directories( ${PROJECT_SOURCE_DIR}/bgapi )
include_directories( ${PROJECT_SOURCE_DIR}/xmodem )
include_directories( ${PROJECT_SOURCE_DIR}/uart-driver )


add_executable( ${EXEC_NAME} ${SOURCES} )


install( 
    TARGETS ${EXEC_NAME} 
    RUNTIME DESTINATION bin 
)