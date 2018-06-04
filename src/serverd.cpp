#include <iostream>
#include "utils.h"
#include "messaging_server.h"
#include <thread>
#include <queue>
#include "account.h"
#include "plog/Log.h"
#include <plog/Appenders/ColorConsoleAppender.h>
#include <src/cblib/request_methods.h>
#include "cb_tests.h"
#include <zmqpp/context.hpp>
#include <zmqpp/socket.hpp>
#include <zmqpp/message.hpp>
#include <zmqpp/reactor.hpp>
#include <zmqpp/curve.hpp>
#include <zmqpp/zmqpp.hpp>

int main(int argc , char* argv[])
{
    // using plog library for logging purposes.
    //https://github.com/SergiusTheBest/plog

    plog::init(plog::verbose, "log_server.out", 1000000, 5);
    static plog::ColorConsoleAppender<plog::TxtFormatter> consoleAppender;
    plog::init(plog::verbose, &consoleAppender);


    LOG_VERBOSE << "The server main process starts.";

    pid_t pid; //process ID
    pid_t sid; // session ID

    //TODO: daemon mode
    //EnvironmentSetup(pid, sid);
    //clear_db();



    // setup wallet
    methods_setup();
    ServerEngine user_wallet(false);


    // TODO: modify test case activation
    LOG_VERBOSE << "development unit tests starts.";
    start_tests();

    zmqpp::context context;

    zmqpp::auth authenticator(context);

    // get detials of authentication TODO: get it into LOG system
    authenticator.set_verbose(true);

    authenticator.configure_domain("*");
    authenticator.configure_curve("CURVE_ALLOW_ANY");


    zmqpp::socket frontend(context, zmqpp::socket_type::router);
    zmqpp::socket backend(context, zmqpp::socket_type::router);

    LOG_DEBUG << "Server secret key: " << user_wallet.get_keys().secret_key;
    LOG_DEBUG << "Server public key: " << user_wallet.get_keys().public_key;

    int as_server = 1;
    frontend.set(zmqpp::socket_option::curve_server, as_server);
    frontend.set(zmqpp::socket_option::curve_secret_key, user_wallet.get_keys().secret_key);


    frontend.bind("tcp://*:9255");    //  For clients
    backend.bind ("ipc://server_workers.ipc");    //  For internal workers


    //  Queue of available workers
    queue<worker_t> workerQueue;

    std::thread worker_thread[WORKER_NUM];

    for(int i=0; i < WORKER_NUM; i++)
    {
        worker_thread[i] = std::thread(worker_t::worker_routine, &context);
        LOG_INFO << "The" << i << "th worker thread created.";
    }

    //  Send out heartbeats at regular intervals
    int64_t heartbeat_at = s_clock () + HEARTBEAT_INTERVAL;


    zmqpp::reactor reactor;
    zmqpp::poller& poller = reactor.get_poller();

    auto socket_listener = [&poller, &backend, &frontend, &workerQueue, &heartbeat_at](){


        // worker

        //  Handle worker activity on backend
        if (poller.has_input(backend)) {
            zmqpp::message msg;
            backend.receive(msg);

            //the unwrap method somehow drops two frames(parts)
            //from message which is not correct
            //so the pop method hard coded instead.
            std::string identity = msg.get(0);



            //  Return reply to client if it's not a control message
            if (msg.parts () == 2) {
                if (strcmp (msg.get(1).c_str(), "READY") == 0) {
                    workerQueue._delete(identity);
                    workerQueue._append(identity);
                }
                else
                {
                    if (strcmp(msg.get(1).c_str(), "HEARTBEAT") == 0) {
                        workerQueue._refresh(identity);
                    } else{
                        LOG_WARNING << "invalid message from " << identity;
                    }
                }
            }
            else{
                LOG_INFO << "Message received from worker " << identity << " and sent to client.";
                msg.remove(0);
                frontend.send(msg);
                workerQueue._append(identity);
            }
        }

        // worker

        //client

        if (poller.has_input(frontend) && workerQueue.size()) {
            //  Now get next client request, route to next worker
            zmqpp::message dealer_in;
            zmqpp::message reply;

            frontend.receive(dealer_in);

            for(int i =0 ; i < dealer_in.parts() ; i++)
            {
                reply.push_back(dealer_in.get(i));
            }

            std::string dealer_identity, dealer_empty, dealer_payload, dealer_user_id;
            dealer_in >> dealer_identity;
            dealer_in.pop_front();
            dealer_in.reset_read_cursor();
            dealer_in >> dealer_empty >> dealer_payload;
            bool dealer_status = dealer_in.get_property("User-Id", dealer_user_id);


            LOG_INFO << "request received from user id: " << dealer_user_id;

            std::string identity = std::string(workerQueue._dequeue());
            reply.push_front((char*)dealer_user_id.c_str()); //TODO
            reply.push_front((char*)identity.c_str());
            backend.send(reply);
        }

        //client

        //  Send heartbeats to idle workers if it's time
        if (s_clock () > heartbeat_at) {
            for (std::vector<worker_t>::iterator it = workerQueue.get_queue().begin();
                 it < workerQueue.get_queue().end(); it++) {
                zmqpp::message msg("HEARTBEAT");
                msg.push_front(it->get_identity().c_str());
                backend.send(msg);
            }
            heartbeat_at = s_clock () + HEARTBEAT_INTERVAL;
        }
        workerQueue._purge();

    };


    reactor.add( frontend, socket_listener );
    reactor.add( backend, socket_listener );

    while(reactor.poll()) {}

    for(int i=0; i < WORKER_NUM; i++)
    {
        worker_thread[i].join();
    }
    LOG_INFO << "daemon main process finished.";
    return 0;
}




//test

//#include <jsoncpp/json/json.h>
//#include "cb_tests.h"
//#include <iostream>
//
//
//int main()
//{
//
//    //using plog library for logging purposes.
//    //https://github.com/SergiusTheBest/plog
//
//    plog::init(plog::verbose, "log.out", 1000000, 5);
//    static plog::ColorConsoleAppender<plog::TxtFormatter> consoleAppender;
//    plog::init(plog::verbose, &consoleAppender);
//
//    start_tests();
//}