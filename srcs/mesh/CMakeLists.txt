FIND_PACKAGE(CGAL REQUIRED)
IF(CGAL_FOUND)
    MESSAGE(STATUS "Found CGAL: " ${CGAL_DIR})
ENDIF()

SET(LIBMESHSRCS
./Mesh.cpp ./Mesh2D.cpp ./Mesh3D.cpp
./Mesh.hpp ./Mesh.inl ./Node.hpp)

ADD_LIBRARY(pfemMesh SHARED ${LIBMESHSRCS})
GENERATE_EXPORT_HEADER(pfemMesh
                       EXPORT_MACRO_NAME MESH_API
					   EXPORT_FILE_NAME Mesh_export.h)
TARGET_INCLUDE_DIRECTORIES(pfemMesh SYSTEM
                           PUBLIC ${CMAKE_CURRENT_BINARY_DIR}
                           PRIVATE ${GMSH_INCLUDE_DIRS})
TARGET_LINK_LIBRARIES(pfemMesh PRIVATE CGAL::CGAL ${GMSH_LIBRARIES})
SET_TARGET_PROPERTIES(pfemMesh PROPERTIES COMPILE_FLAGS "-Wall -Wextra -pedantic-errors -Wold-style-cast -Wduplicated-cond -Wduplicated-branches -Wlogical-op -Wnull-dereference -Wuseless-cast -Wshadow")
