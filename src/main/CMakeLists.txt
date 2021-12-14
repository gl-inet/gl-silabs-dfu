set( EXEC_NAME "gl-silabs-dfu" )

aux_source_directory(./ SOURCES)
aux_source_directory(../driver SOURCES)

include_directories(./ )
include_directories( ${PROJECT_SOURCE_DIR}/driver )

add_executable( ${EXEC_NAME} ${SOURCES} )


install( 
    TARGETS ${EXEC_NAME} 
    RUNTIME DESTINATION bin 
)