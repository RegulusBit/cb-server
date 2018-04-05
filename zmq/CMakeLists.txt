add_library(zmq messaging_server.cpp zmsg.hpp)

include_directories("/usr/local/include/mongocxx/v_noabi")
include_directories("/usr/local/include/bsoncxx/v_noabi")

target_include_directories(zmq PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ../plog/include)

target_link_libraries(zmq utilities mongo_db)
