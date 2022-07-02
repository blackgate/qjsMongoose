cmake_minimum_required(VERSION 3.0.0)
project(qjsMongoose VERSION 0.1.0)

include_directories(${PROJECT_SOURCE_DIR}/src)
file(GLOB SRC_FILES ${PROJECT_SOURCE_DIR}/src/*.c)

find_library(OPEN_SSL_LIB ssl)

add_library(qjsMongoose SHARED ${SRC_FILES})

target_link_libraries(qjsMongoose PRIVATE "${OPEN_SSL_LIB}")

add_compile_definitions(JS_SHARED_LIBRARY)
add_compile_definitions(MG_ENABLE_OPENSSL)

install(TARGETS qjsMongoose DESTINATION lib)