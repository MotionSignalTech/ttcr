cmake_minimum_required(VERSION 3.10)
project( ttcr )

add_subdirectory(ttcr)


#########################################
# Python wrapper
set(INSTALL_CODE "execute_process(")
set(INSTALL_CODE "${INSTALL_CODE} COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/ttcrpy/setup.py build_ext --inplace")
set(INSTALL_CODE "${INSTALL_CODE} install --prefix=${CMAKE_INSTALL_PREFIX}")
set(INSTALL_CODE "${INSTALL_CODE} WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/ttcrpy")
set(INSTALL_CODE "${INSTALL_CODE} )")
message(STATUS "install = ${INSTALL_CODE}")
install(CODE "${INSTALL_CODE}")
