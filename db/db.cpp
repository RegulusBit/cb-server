//
// Created by reza on 4/4/18.
//

#include "db.h"


mongocxx::instance database::instance{};
mongocxx::client database::client{mongocxx::uri{}};
Json::Reader database::rdr;

bool database::add_request(Json::Value request)
{
    std::string requestStr;
    try {
        rdr.parse(requestStr, request);

        bsoncxx::document::value doc_value = bsoncxx::from_json(requestStr);
        bsoncxx::document::view view = doc_value.view();

        mongocxx::database db = client["mydb"];
        mongocxx::collection coll = db["test"];

        bsoncxx::stdx::optional<mongocxx::result::insert_one> result =
                coll.insert_one(view);

    }catch (...)
    {
        return false;
    }

    return true;
}

bool database::add_request(std::string request)
{

    //try {
        LOG_DEBUG << "database additon process started.";

        bsoncxx::document::value doc_value = bsoncxx::from_json(request);
        bsoncxx::document::view view = doc_value.view();
        LOG_DEBUG << "request string converted to mongodb compatible JSON.";

        mongocxx::database db = client["mydb"];
        LOG_DEBUG << "db successfully created with client.";

        mongocxx::collection coll = db["test"];
        LOG_DEBUG << "db successfully connected.";

        bsoncxx::stdx::optional<mongocxx::result::insert_one> result =
                coll.insert_one(view);
        LOG_DEBUG << "requested data inserted to " << coll.name() << " in " << db.name();

    auto cursor = coll.find({});

    for (auto&& doc : cursor) {
        std::cout << bsoncxx::to_json(doc) << std::endl;
    }

   // }catch (...)
   // {
   //     LOG_DEBUG << "exception occurred!";
    //    return false;
   // }

    return true;
}
