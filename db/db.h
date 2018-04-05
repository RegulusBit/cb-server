//
// Created by reza on 4/4/18.
//

#ifndef CB_SERVER_DB_H
#define CB_SERVER_DB_H

#include <cstdint>
#include <iostream>
#include <vector>
#include <bsoncxx/json.hpp>
#include <bsoncxx/builder/basic/array.hpp>
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/stdx.hpp>
#include <mongocxx/uri.hpp>
#include <jsoncpp/json/json.h>
#include "plog/Log.h"
#include <plog/Appenders/ColorConsoleAppender.h>


using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class database
{
public:

    static bool add_request(Json::Value request);
    static bool add_request(std::string request);

private:

    database();

    static mongocxx::instance instance;
    static mongocxx::client client;
    static Json::Reader rdr;
};


#endif //CB_SERVER_DB_H
