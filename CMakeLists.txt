cmake_minimum_required(VERSION 3.9)
project(cb_server)


set(CMAKE_CXX_STANDARD 11)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

## load in pkg-config support
find_package(PkgConfig)
find_package(libmongocxx REQUIRED)
find_package(libbsoncxx REQUIRED)
find_package(Threads)
pkg_check_modules(JSONCPP jsoncpp)


add_subdirectory(db)
add_subdirectory(zmq)
add_subdirectory(utilities)
add_subdirectory(zclib)

## use pkg-config to get hints for 0mq locations
pkg_check_modules(PC_ZeroMQ QUIET zmq)

## use the hint from above to find where 'zmq.hpp' is located
find_path(ZeroMQ_INCLUDE_DIR
        NAMES zmq.hpp
        PATHS ${PC_ZeroMQ_INCLUDE_DIRS}
        )

## use the hint from about to find the location of libzmq
find_library(ZeroMQ_LIBRARY
        NAMES zmq
        PATHS ${PC_ZeroMQ_LIBRARY_DIRS}
        )



add_executable(cb_server main.cpp)

include_directories(${LIBMONGOCXX_INCLUDE_DIR})
include_directories(${LIBBSONCXX_INCLUDE_DIR})

#it's not good to hard code absolute paths but it's needed
#https://stackoverflow.com/questions/37210716/using-the-mongodb-cxx-driver-in-a-cmake-c-project?utm_medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa

include_directories("/usr/local/include/mongocxx/v_noabi")
include_directories("/usr/local/include/bsoncxx/v_noabi")

target_link_libraries(cb_server mongo_db utilities zclib zmq ${CMAKE_THREAD_LIBS_INIT} ${ZeroMQ_LIBRARY} ${LIBMONGOCXX_LIBRARIES} ${JSONCPP_LIBRARIES} ${LIBBSONCXX_LIBRARIES})

target_include_directories(cb_server PUBLIC plog/include ${ZeroMQ_INCLUDE_DIR})


#
# mongoDB installation process:
# [1...3] is deprecated:
# 1) sudo apt-key adv --keyserver hkp://keyserver.ubuntu.com:80 --recv 7F0CEB10
# 2) echo "deb http://repo.mongodb.org/apt/ubuntu xenial/mongodb-org/3.2 multiverse" | sudo tee /etc/apt/sources.list.d/mongodb-org-3.2.list
# 3) sudo apt-get update
# it's restricted in Iran! so just install it from the standard repositories:
# 4) sudo apt-get install mongodb
# 5) sudo service mongodb start
#
# https://www.tutorialspoint.com/mongodb/mongodb_data_modeling.htm
#
# install c and cpp drivers:
# 1) wget https://github.com/mongodb/libbson/releases/download/1.9.3/libbson-1.9.3.tar.gz
#    tar -xzf libbson-1.9.3.tar.gz
#    cd libbson-1.9.3/
#    ./configure
#    wget https://github.com/mongodb/mongo-c-driver/releases/download/1.9.3/mongo-c-driver-1.9.3.tar.gz
#    tar xzf mongo-c-driver-1.9.3.tar.gz
#    cd mongo-c-driver-1.9.3
#    ./configure --disable-automatic-init-and-cleanup
# 2) curl -OL https://github.com/mongodb/mongo-cxx-driver/archive/r3.1.2.tar.gz
# 3) tar -xzf r3.1.2.tar.gz
# 4) cd mongo-cxx-driver-r3.1.2/build
# 5) cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local ..
# 6) make && sudo make install
#
# Note: there was an error in build process. you should add the #include<cassert>
#       in the mock.hh. the path to file is provided in build error log in bash
#
# Note: error while loading shared libraries: libmongocxx.so._noabi: cannot open shared object file
#       solution: sudo /sbin/ldconfig -v
