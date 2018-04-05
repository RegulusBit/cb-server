add_library(utilities utils.cpp)

include_directories("/usr/local/include/mongocxx/v_noabi")
include_directories("/usr/local/include/bsoncxx/v_noabi")

target_include_directories(utilities PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ../plog/include)

target_link_libraries(utilities PUBLIC zclib jsoncpp mongo_db ${Boost_LIBRARIES})
