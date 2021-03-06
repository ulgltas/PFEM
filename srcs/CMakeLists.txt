INCLUDE(GenerateExportHeader)

ADD_SUBDIRECTORY(mesh)

ADD_SUBDIRECTORY(solver)

SET(SRCS
main.cpp)

FIND_PATH(JSON_INCLUDE_DIRS NAMES "nlohmann/json.hpp")
MESSAGE(STATUS "Found nlohmann json: " ${JSON_INCLUDE_DIRS})
if(NOT JSON_INCLUDE_DIRS)
    MESSAGE(FATAL_ERROR "nlohmann json not found!")
ENDIF()

ADD_EXECUTABLE(pfem ${SRCS})
TARGET_INCLUDE_DIRECTORIES(pfem SYSTEM
                           PUBLIC ${CMAKE_CURRENT_BINARY_DIR} ${JSON_INCLUDE_DIRS})
TARGET_LINK_LIBRARIES(pfem PUBLIC pfemSolver)
SET_TARGET_PROPERTIES(pfem PROPERTIES COMPILE_FLAGS "-Wall -Wextra -pedantic-errors -Wold-style-cast -Wduplicated-cond -Wduplicated-branches -Wlogical-op -Wnull-dereference -Wuseless-cast -Wshadow")