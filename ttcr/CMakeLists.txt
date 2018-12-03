
#########################################
# Find Eigen

find_package (Eigen3 3 REQUIRED NO_MODULE)
INCLUDE_DIRECTORIES ( "${EIGEN3_INCLUDE_DIR}" )

#########################################
# Find Boost

set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)

find_package(Boost 1.58.0 REQUIRED)

if(NOT Boost_FOUND)
    MESSAGE(FATAL_ERROR "Unable to find correct Boost version. Did you set BOOST_ROOT?")
endif()

include_directories("${Boost_INCLUDE_DIRS}")
message(STATUS "Boost_LIBRARIES = ${Boost_LIBRARIES}")

#########################################
# Find VTK

find_package(VTK 8.0 REQUIRED NO_MODULE)

include(${VTK_USE_FILE})
message(STATUS "VTK_LIBRARIES = ${VTK_LIBRARIES}")
message(STATUS "VTK_INSTALL_PREFIX = ${VTK_INSTALL_PREFIX}")

add_definitions(-DVTK)

#########################################
# Building 

#set(CMAKE_CXX_FLAGS "-std=c++11 -march=native")
set(CMAKE_CXX_FLAGS "-std=c++11")

set( ttcr3d_SRCS ttcr3d.cpp ttcr_io.cpp )
set( ttcr2d_SRCS ttcr2d.cpp ttcr_io.cpp )
set( ttcr2ds_SRCS ttcr2ds.cpp ttcr_io.cpp )
set( CMAKE_VERBOSE_MAKEFILE on )

add_executable( ttcr3d ${ttcr3d_SRCS} )
add_executable( ttcr2d ${ttcr2d_SRCS} )
add_executable( ttcr2ds ${ttcr2ds_SRCS} )

target_link_libraries(ttcr3d ${VTK_LIBRARIES} )#${C++_LIBRARY})
target_link_libraries(ttcr2d ${VTK_LIBRARIES} )#${C++_LIBRARY})
target_link_libraries(ttcr2ds ${VTK_LIBRARIES} )#${C++_LIBRARY})

set_property(TARGET ttcr3d ttcr2d ttcr2ds PROPERTY INSTALL_RPATH_USE_LINK_PATH TRUE)

install(TARGETS ttcr3d ttcr2d ttcr2ds RUNTIME DESTINATION bin)



