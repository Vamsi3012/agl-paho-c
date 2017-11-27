PROJECT_TARGET_ADD(agl-paho-c)

#Link to the MQTT PAHO Shared library.
LINK_DIRECTORIES(/usr/local/lib)

include_directories(src)

file(GLOB SOURCES "src/*.c")
add_library(
  ${TARGET_NAME} SHARED
  MODULE ${SOURCES}
  
)

# Binder exposes a unique public entry point
SET_TARGET_PROPERTIES(agl-paho-c PROPERTIES
	PREFIX "afb-"
        LABELS "BINDING"
	LINK_FLAGS  ${BINDINGS_LINK_FLAG}
        OUTPUT_NAME ${TARGET_NAME}
)


target_link_libraries(${TARGET_NAME} ${link_libraries} libpaho-mqtt3c.so)







