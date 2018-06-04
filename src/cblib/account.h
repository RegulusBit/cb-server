//
// Created by reza on 4/13/18.
//

#ifndef TUTSERVER_ACCOUNT_H
#define TUTSERVER_ACCOUNT_H


#include <string>
#include <vector>
#include <cstdint>
#include <unistd.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <iostream>
#include <sstream>
#include <ctime>
#include <unordered_map>
#include <chrono>
#include <jsoncpp/json/value.h>
#include <jsoncpp/json/json.h>
#include "address.h"
#include "plog/Log.h"
#include <plog/Appenders/ColorConsoleAppender.h>
#include <unordered_map>
#include "db/db.h"
#include "src/utilities/utils.h"
#include <zmqpp/zmqpp.hpp>
#include <zmqpp/curve.hpp>
#include <zcash/JoinSplit.hpp>
#include "src/cblib/note.h"

// defining pointer to request processing functions
typedef Json::Value (*request_ptr)(jsonParser parser, std::string user_id);


class Account {
public:
    Account();
    Account(std::string account_json_string);
    Account(__uint64_t balance);
    double get_balance(void);

//TODO:
//    ~Account()
//    {
//        if(paymentAddress)
//            delete paymentAddress;
//    }

    std::string serialize(void);
    CBPaymentAddress* get_paymentAddress(void){return paymentAddress;}

private:

    // uint256 Id; required to specifically point to any account
    //uint256 implementation needed

    std::string account_description;
    CBPaymentAddress* paymentAddress;
    double Balance;

};


class Keypair
{

public:
    Keypair(){};
    Keypair(std::string keypair_json_string);
    std::string serialize(void);
    void generate_keypair(void);

    std::string secret_key() { return key_pair.secret_key;}
    std::string public_key() { return key_pair.public_key;}
    zmqpp::curve::keypair get_keys(void) {return key_pair;}

private:

    zmqpp::curve::keypair key_pair;
};

class ServerEngine
{

public:

    ServerEngine(bool is_test);
    ~ServerEngine()
    {
        //TODO: memory management sucks! garbage collection mechanism needs modification
        //if(default_account)
            //delete default_account;
    }
    zmqpp::curve::keypair get_keys(void);
    ZCJoinSplit* get_zkparams(void){return pzcashParams;}
    ZCIncrementalMerkleTree get_merkle(void)
    {
        if(!sync_merkle())
            LOG_ERROR << "merkle synchronization failed";

        return merkleTree.get_merkle();
    }
    bool update_merkle(uint256 commitment);
    bool update_merkle(ZCIncrementalMerkleTree new_merkle);
    bool delete_keys(void);

    Account& get_pool_account(void);




private:

    bool test;
    database* server_db;
    Keypair server_keypair;
    MerkleTree merkleTree;
    ZCJoinSplit *pzcashParams = NULL;
    Account poolAccount;

    bool sync_engine(void);
    bool sync_keypair(void);
    bool sync_poolaccount(void);
    bool fetch_zparams(void);
    bool sync_merkle(void);

};





#endif //TUTSERVER_ACCOUNT_H
