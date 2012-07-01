IF(NOT RPCGEN_EXECUTABLE)
	MESSAGE(STATUS "Looking for rpcgen")
	FIND_PROGRAM(RPCGEN_EXECUTABLE rpcgen)
	IF(RPCGEN_EXECUTABLE)
		EXECUTE_PROCESS(COMMAND "${RPCGEN_EXECUTABLE}" --version OUTPUT_VARIABLE _version)
		STRING(REGEX MATCH "[0-9.]+" RPCGEN_VERSION ${_version})
		SET(RPCGEN_FOUND TRUE)
	ENDIF(RPCGEN_EXECUTABLE)
ELSE(NOT RPCGEN_EXECUTABLE)
	EXECUTE_PROCESS(COMMAND "${RPCGEN_EXECUTABLE}" --version OUTPUT_VARIABLE _version)
	STRING(REGEX MATCH "[0-9.]+" RPCGEN_VERSION ${_version})
	SET(RPCGEN_FOUND TRUE)
ENDIF(NOT RPCGEN_EXECUTABLE)

IF(RPCGEN_FOUND)
	MESSAGE(STATUS "Found rpcgen: ${RPCGEN_EXECUTABLE} (${RPCGEN_VERSION})")

	IF(NOT RPCGEN_FLAGS)
		SET(RPCGEN_FLAGS "")
	ENDIF(NOT RPCGEN_FLAGS)

	MACRO(RPCGEN_CREATE_XDR SRCFILE)
		GET_FILENAME_COMPONENT(SRCPATH "${SRCFILE}" PATH)
		GET_FILENAME_COMPONENT(SRCBASE "${SRCFILE}" NAME_WE)
		SET(OUTFILE1 "${CMAKE_CURRENT_BINARY_DIR}/${SRCPATH}/${SRCBASE}.h")
		SET(OUTFILE2 "${CMAKE_CURRENT_BINARY_DIR}/${SRCPATH}/${SRCBASE}.c")
		SET(OUTFILE3 "${CMAKE_CURRENT_BINARY_DIR}/${SRCPATH}/${SRCBASE}_xdr.c")
		FILE(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/${SRCPATH}")
		SET(INFILE "${SRCFILE}")
		SET(_flags ${ARGV1})
		IF(NOT _flags)
			SET(_flags ${RPCGEN_FLAGS})
		ENDIF(NOT _flags)
		ADD_CUSTOM_COMMAND(OUTPUT ${OUTFILE1}
			COMMAND "${RPCGEN_EXECUTABLE}"
			ARGS -h ${_flags} -o "${OUTFILE1}" "${INFILE}"
			DEPENDS "${INFILE}"
			WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
			COMMENT "Generating ${SRCBASE}.h from ${SRCFILE}"
		)
		ADD_CUSTOM_COMMAND(OUTPUT ${OUTFILE2}
			COMMAND "${RPCGEN_EXECUTABLE}"
			ARGS -c ${_flags} -o "${OUTFILE2}" "${INFILE}"
			DEPENDS "${INFILE}"
			WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
			COMMENT "Generating ${SRCBASE}.c from ${SRCFILE}"
		)
	ENDMACRO(RPCGEN_CREATE_XDR)

ELSE(RPCGEN_FOUND)
	MESSAGE(FATAL_ERROR "Could not find rpcgen")
ENDIF(RPCGEN_FOUND)