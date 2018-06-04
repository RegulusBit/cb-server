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
#include "src/utilities/utils.h"
#include <mongocxx/pool.hpp>
#include <zmq.hpp>
#include <src/zmq/zhelpers.hpp>
#include <src/zmq/messaging_server.h>
#include "src/cblib/process_request.h"

using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

// TODO: this method must be deployed
// TODO: (source: https://stackoverflow.com/questions/201593/is-there-a-simple-way-to-convert-c-enum-to-string#201792)
/*
#define SOME_ENUM(DO) \
    DO(accounts) \
    DO(aaaa) \
    DO(baaa)

#define SOME_ENUM(DO) \
    DO(Foo) \
    DO(Bar) \
    DO(Baz)

#define MAKE_ENUM(VAR) VAR,
enum MetaSyntacticVariable{
    SOME_ENUM(MAKE_ENUM)
};

#define MAKE_STRINGS(VAR) #VAR,
const char* const MetaSyntacticVariableNames[] = {
        SOME_ENUM(MAKE_STRINGS)

};
*/

enum collection {accounts, key_pair,notes, nullifiers, merkle_tree, merkle_tree_test, accounts_test, notes_test};
static std::vector<std::string> collector = {"accounts","key_pair", "notes", "nullifiers", "merkle_tree", "merkle_tree_test", "accounts_test", "notes_test"};




class database
{
public:

    static database& get_instance();

    bool add_request(Json::Value request, std::string collection_name, std::string user_id);
    bool add_request(std::string request, std::string collection_name, std::string user_id);

    bool delete_request(collection result_type,
                        bsoncxx::document::view_or_value filter,
                        int* deleted_number = nullptr);

    template <typename T>
    bool query(std::vector<T> &result_vector,
               collection result_type,
               bsoncxx::document::view_or_value filter,
               mongocxx::options::find options,
               std::string user_id, bool is_userbased);

    bool update(collection result_type,
                bsoncxx::document::view_or_value filter,
                bsoncxx::document::view_or_value update,
                std::string user_id);

    mongocxx::database& get_db()
    {
        auto client = pool.acquire();
        db = (*client)["serverDB"];
        return db;
    }

    static void worker_routine(void* context);
    static zmq::socket_t *s_db_socket(zmq::context_t &context);

    std::string update_db(std::string);

    database(database const&) = delete;
    void operator=(database const&) = delete;

private:

    database();

    mongocxx::instance instance;
    mongocxx::pool pool;
    mongocxx::database db;
    Json::Reader rdr;
};



template <typename T>
bool database::query(std::vector<T> &result_vector,
                     collection result_type,
                     bsoncxx::document::view_or_value filter,
                     mongocxx::options::find options,
                     std::string user_id, bool is_userbased)
{
    auto client = pool.acquire();
    mongocxx::database db = (*client)["serverDB"];
    mongocxx::collection coll = db.collection(collector[result_type]);
    mongocxx::cursor curser = coll.find(filter, options);


    bool has_element = false;
    for(auto doc : curser)
    {
        Json::Value result;
        Json::Reader rdr;
        rdr.parse(bsoncxx::to_json(doc), result);

        if(result["user_id"] != user_id && is_userbased)
            continue;
        else{
            T temp(bsoncxx::to_json(doc));
            result_vector.push_back(temp);
            has_element = true;
        }
    }

    return has_element;
}


#endif //CB_SERVER_DB_H
