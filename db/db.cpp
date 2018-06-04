//
// Created by reza on 4/4/18.
//



#include "db.h"


//TODO: it's probably hitting the logical error
database::database():instance(),
                     pool(mongocxx::uri()),
                     db()
{

    get_db();
}


database& database::get_instance()
{
    static database instance;

    return instance;
}



bool database::add_request(std::string request, std::string collection_name, std::string user_id)
{
    //TODO
   // try {

        Json::Value temp;
        Json::Reader rdr;
        Json::FastWriter fst;
        rdr.parse(request, temp);
        temp["user_id"] = user_id;
        request = fst.write(temp);

        LOG_DEBUG << "database addition process started.";
        LOG_DEBUG << "string to be inserted: " << request;

        bsoncxx::document::value doc_value = bsoncxx::from_json(request);
        bsoncxx::document::view view = doc_value.view();
        LOG_DEBUG << "request string converted to mongodb compatible JSON.";

        // TODO: it has to be replaced with something reusable.
        auto client = pool.acquire();
        db = (*client)["serverDB"];

        mongocxx::collection coll = db[collection_name];
        LOG_DEBUG << "db successfully connected.";

        bsoncxx::stdx::optional<mongocxx::result::insert_one> result =
                coll.insert_one(view);
        LOG_DEBUG << "requested data inserted to " << coll.name() << " in " << db.name();

    auto cursor = coll.find({});

    for (auto&& doc : cursor) {
        LOG_WARNING << "SUCCESS: " << bsoncxx::to_json(doc);
    }

   // }catch (...)
   // {
   //     Exception("adding to database failed.");
   //     return false;
   // }

    return true;
}


bool database::delete_request(collection result_type,
                    bsoncxx::document::view_or_value filter,
                    int* deleted_number)
{
    try {

        LOG_DEBUG << "database deleting process started.";
        LOG_DEBUG << "request string converted to mongodb compatible JSON.";

        // TODO: it has to be replaced with something reusable.
        auto client = pool.acquire();
        db = (*client)["serverDB"];

        mongocxx::collection coll = db.collection(collector[result_type]);
        LOG_DEBUG << "db successfully connected.";

        // deleting process

        bsoncxx::stdx::optional<mongocxx::result::delete_result> result = coll.delete_many(filter);
        if(deleted_number != nullptr)
            *deleted_number = result->deleted_count();

        // #deleting process

    }catch (...)
    {
        Exception("deleting from database failed.");
        return false;
    }

    return true;

}




bool database::update(collection result_type,
                      bsoncxx::document::view_or_value filter,
                      bsoncxx::document::view_or_value update,
                      std::string user_id)
{

    try {
        auto client = pool.acquire();
        mongocxx::database db = (*client)["serverDB"];
        mongocxx::collection coll = db.collection(collector[result_type]);

        LOG_DEBUG << "db successfully connected for update.";

        bsoncxx::stdx::optional<mongocxx::result::update> result = coll.update_many(filter, update);


        if(result)
            return true;
        else
            return false;

    }catch(...)
    {
        Exception("error occurred in updating database.");
        return false;
    }

}





// TODO: create specific thread for database
zmq::socket_t* database::s_db_socket(zmq::context_t &context)
{
    zmq::socket_t * worker = new zmq::socket_t(context, ZMQ_DEALER);

    //  Set random identity to make tracing easier
    std::string identity = s_set_id(*worker);
    worker->connect ("tcp://localhost:8888");

    //  Configure socket to not wait at close time
    int linger = 0;
    worker->setsockopt(ZMQ_LINGER, &linger, sizeof(linger));

    //  Tell queue we're ready for work
    LOG_INFO << " (" << identity << ") db_worker ready";
    s_send(*worker, "READY");

    return worker;
}

//void database::worker_routine(void* context)
//{
//    zmq::context_t* cntxt = static_cast<zmq::context_t*>(context);
//    srandom((unsigned) time(NULL));
//    zmq::socket_t * worker = s_db_socket(*cntxt);
//
//    //  If liveness hits zero, queue is considered disconnected
//    size_t liveness = HEARTBEAT_LIVENESS;
//    size_t interval = INTERVAL_INIT;
//
//    //  Send out heartbeats at regular intervals
//    int64_t heartbeat_at = s_clock() + HEARTBEAT_INTERVAL;
//
//    //queue
//
//
//    while(true) {
//        zmq::pollitem_t items[] = { { (void*)*worker,  0, ZMQ_POLLIN, 0 } };
//        zmq::poll (items, 1, HEARTBEAT_INTERVAL);
//
//        if(items[0].revents & ZMQ_POLLIN) {
//            //  Get message
//            //  - 3-part envelope + content -> request
//            //  - 1-part "HEARTBEAT" -> heartbeat
//
//
//            zmsg msg(*worker);
//
//            if (msg.parts() == 3) {
//                //  Process the request and send reply
//                LOG_INFO << "normal reply - " << msg.body();
//                zmsg reply(msg);
//                reply.body_set(processRequest(msg.body()).c_str());
//                reply.send(*worker);
//                liveness = HEARTBEAT_LIVENESS;
//                //  Do some heavy work
//            }
//            else {
//                if(msg.parts () == 1 && strcmp (msg.body (), "HEARTBEAT") == 0) {
//                    liveness = HEARTBEAT_LIVENESS;
//                }
//                else {
//                    LOG_INFO << "invalid message";
//                    msg.dump ();
//                }
//            }
//            interval = INTERVAL_INIT;
//
//        }
//        else
//        if (--liveness == 0) {
//            LOG_WARNING << "heartbeat failure, can't reach queue";
//            LOG_WARNING << "reconnecting in " << interval << " msec…";
//            s_sleep(interval);
//
//            if(interval < INTERVAL_MAX) {
//                interval *= 2;
//            }
//            delete worker;
//            worker = s_db_socket(*cntxt);
//            liveness = HEARTBEAT_LIVENESS;
//        }
//
//        //  Send heartbeat to queue if it's time
//        if (s_clock() > heartbeat_at) {
//            heartbeat_at = s_clock () + HEARTBEAT_INTERVAL;
//            //std::cout << "I: (" << identity << ") worker heartbeat" << std::endl;
//            s_send(*worker, "HEARTBEAT");
//        }
//    }
//    delete worker;
//
//    //queue
//}
//
//std::string database::update_db(std::string)
//{
//    // TODO: indirect access to database through db_worker thread must be implemented.
//    return "result";
//}
