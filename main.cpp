#include <iostream>
#include "utils.h"
#include "messaging_server.h"
#include <thread>
#include <queue>
#include "plog/Log.h"
#include <plog/Appenders/ColorConsoleAppender.h>



int main(int argc, char* argv[])
{

    plog::init(plog::verbose, "log.out", 1000000, 5);
    static plog::ColorConsoleAppender<plog::TxtFormatter> consoleAppender;
    plog::init(plog::verbose, &consoleAppender);

    LOG_VERBOSE << "The server main process starts.";

    zmq::context_t context(1);
    zmq::socket_t frontend(context, ZMQ_ROUTER);
    zmq::socket_t backend (context, ZMQ_ROUTER);

    frontend.bind("tcp://*:9155");    //  For clients
    backend.bind ("tcp://*:5656");    //  For internal workers


    //  Queue of available workers
    queue<worker_t> workerQueue;

    std::thread worker_thread[WORKER_NUM];

    for(int i=0; i < WORKER_NUM; i++)
    {
        worker_thread[i] = std::thread(worker_t::worker_routine, &context);
        LOG_INFO << "The " << i+1 << "th worker thread created.";
    }

    //  Send out heartbeats at regular intervals
    int64_t heartbeat_at = s_clock () + HEARTBEAT_INTERVAL;

    while(true)
    {
        zmq::pollitem_t items[] = {
                {(void*)backend,  0, ZMQ_POLLIN, 0},
                {(void*)frontend, 0, ZMQ_POLLIN, 0}
        };
        //  Poll frontend only if we have available workers
        if (workerQueue.size()) {
            zmq::poll(items, 2, HEARTBEAT_INTERVAL);
        } else {
            LOG_INFO << "The worker queue is empty.";
            zmq::poll(items, 1, HEARTBEAT_INTERVAL);
        }

        // worker

        //  Handle worker activity on backend
        if (items[0].revents & ZMQ_POLLIN) {
            zmsg msg(backend);

            //the unwrap method somehow drops two frames(parts)
            //from message which is not correct
            //so the pop method hard coded instead.
            //std::string identity(msg.unwrap());
            std::string identity = (char*)msg.pop_front().c_str();

            //  Return reply to client if it's not a control message
            if (msg.parts () == 1) {
                if (strcmp (msg.address (), "READY") == 0) {
                    workerQueue._delete(identity);
                    workerQueue._append(identity);
                }
                else
                {
                    if (strcmp(msg.address(), "HEARTBEAT") == 0) {
                        workerQueue._refresh(identity);
                    } else{
                        LOG_WARNING << "invalid message from " << identity;
                        msg.dump ();
                    }
                }
            }
            else{
                LOG_INFO << "Message received from client " << identity << " and sent to worker";
                msg.send(frontend);
                workerQueue._append(identity);
            }
        }

        // worker

        //client

        if (items [1].revents & ZMQ_POLLIN) {
            //  Now get next client request, route to next worker
            zmsg msg(frontend);
            std::string identity = std::string(workerQueue._dequeue());
            msg.push_front((char*)identity.c_str());
            msg.send(backend);
        }

        //client

        //  Send heartbeats to idle workers if it's time
        if (s_clock () > heartbeat_at) {
            for (std::vector<worker_t>::iterator it = workerQueue.get_queue().begin();
                 it < workerQueue.get_queue().end(); it++) {
                zmsg msg ("HEARTBEAT");
                msg.wrap (it->get_identity().c_str(), NULL);
                msg.send (backend);
            }
            heartbeat_at = s_clock () + HEARTBEAT_INTERVAL;
        }
        workerQueue._purge();
    }

    for(int i=0; i < WORKER_NUM; i++)
    {
        worker_thread[i].join();
    }
    LOG_INFO << "daemon main process finished.";


    return 0;
}









// test application:
//
//#include <iostream>
//
//#include <bsoncxx/v_noabi/bsoncxx/builder/stream/document.hpp>
//#include <bsoncxx/v_noabi/bsoncxx/json.hpp>
//
//#include <mongocxx/v_noabi/mongocxx/client.hpp>
//#include <mongocxx/v_noabi/mongocxx/instance.hpp>
//
//
//int main(int, char**) {
//    mongocxx::instance inst{};
//    mongocxx::client conn{mongocxx::uri{}};
//
//    bsoncxx::builder::stream::document document{};
//
//    auto collection = conn["testdb"]["testcollection"];
//    document << "hello" << "world";
//
//    collection.insert_one(document.view());
//    auto cursor = collection.find({});
//
//    for (auto&& doc : cursor) {
//        std::cout << bsoncxx::to_json(doc) << std::endl;
//    }
//}