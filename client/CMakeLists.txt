cmake_minimum_required (VERSION 2.8)

project (client)

set (TAGET kcp_client)

set (EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/bin)

add_definitions(-g)

# Includes
include_directories (${CMAKE_CURRENT_SOURCE_DIR}
					${PROJECT_ROOT_DIR}/kcp
					 )
# Sources
set(SRCS
	${PROJECT_ROOT_DIR}/kcp/ikcp.c
	${CMAKE_CURRENT_SOURCE_DIR}/kcpClient.c
)
add_executable(${TAGET}
			${SRCS}
			)

# target_link_libraries(${TAGET}
# 		)
