PROJECT_TARGET_ADD(agl-paho-c)

#Link to the MQTT PAHO Shared library (The library is installed in this location).
LINK_DIRECTORIES(/usr/local/lib)
#include binder source file directory
include_directories(src)
#include eclipse paho headr file directory (After installing the eclipse paho library, the header files can be found in this directory)
include_directories(/usr/local/include)

#pick all the source files that are needed for building the binder
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

#Link all necessary libraries
target_link_libraries(${TARGET_NAME} ${link_libraries} libpaho-mqtt3c.so)







