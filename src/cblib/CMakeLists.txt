add_library(cblib address.cpp account.cpp request_methods.cpp request_methods.h account.h process_request.h process_request.cpp note.cpp)

target_include_directories(cblib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(cblib PUBLIC utilities mongo_db)
