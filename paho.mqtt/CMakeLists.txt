include(${CMAKE_ROOT}/Modules/ExternalProject.cmake)

set(prefix ${CMAKE_BINARY_DIR}/local)

ExternalProject_Add(
	external-paho-mqtt
		GIT_REPOSITORY https://github.com/eclipse/paho.mqtt.c.git
		GIT_TAG origin/master
		GIT_SHALLOW TRUE
		GIT_PROGRESS TRUE
		CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${prefix}
)

PROJECT_TARGET_ADD(paho-mqtt)

add_library(${TARGET_NAME} SHARED IMPORTED GLOBAL)

add_dependencies(${TARGET_NAME} external-paho-mqtt)

set_target_properties(${TARGET_NAME} PROPERTIES
	LABELS LIBRARY
	IMPORTED_LOCATION ${prefix}/lib64/libpaho-mqtt3c.so
)


