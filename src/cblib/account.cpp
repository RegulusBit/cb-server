//
// Created by reza on 4/13/18.
//

#include "account.h"
#include "db/db.h"



Account::Account()
{
    paymentAddress = new CBPaymentAddress();
    Balance = 0; // default value of deposited account.
}

Account::Account(std::string account_json_string)
{
    Json::Reader rdr;
    Json::Value account_json;
    rdr.parse(account_json_string, account_json);

    if(!account_json.isMember("pk"))
    {
        LOG_ERROR << "the provided string to construct account is not valid";
        LOG_ERROR << "pk is provided: " << account_json.isMember("pk");
        throw Exception("the provided string to construct account is not valid");
    } else
    {
        LOG_DEBUG << "pk is provided: " << account_json.isMember("pk");
        LOG_DEBUG << "pk: " << account_json.get("pk", "NOVALUE").asString();
    }

    try {
        paymentAddress = new CBPaymentAddress(account_json.get("pk", "NOVALUE").asString());
        Balance = account_json.get("balance", 0).asDouble();
    }catch(...)
    {
        throw Exception("payment address cannot be created.");
    }
}

Account::Account(__uint64_t balance)
{
    paymentAddress = new CBPaymentAddress();
    Balance = balance;
}

double Account::get_balance()
{
    return Balance;
}


std::string Account::serialize()
{
    Json::Value serialized;
    Json::Value payment_addr;
    serialized["description"] = account_description;
    serialized["balance"] = Balance;

    // for merging two serialized JSON's. searching in database is easier when
    // JSON's are not nested.
    //serialized["payment_address"] = paymentAddress->serialize();
    for(auto &k: paymentAddress->serialize().getMemberNames())
        serialized[k] = paymentAddress->serialize().get(k, "NOVALUE");

    Json::FastWriter fst;
    return fst.write(serialized);
}

Keypair::Keypair(std::string keypair_json_string)
{
    Json::Reader rdr;
    Json::Value keypair_json;
    rdr.parse(keypair_json_string, keypair_json);

    key_pair.public_key = keypair_json["public_key"].asString();
    key_pair.secret_key = keypair_json["secret_key"].asString();

}

std::string Keypair::serialize()
{
    Json::Value serialized;

    serialized["secret_key"] = key_pair.secret_key;
    serialized["public_key"] = key_pair.public_key;


    Json::FastWriter fst;
    return fst.write(serialized);

}

void Keypair::generate_keypair()
{
    key_pair = zmqpp::curve::generate_keypair();
}



ServerEngine::ServerEngine(bool is_test):server_keypair(),merkleTree(),poolAccount()
{
    this->test = is_test;
    sync_engine();
}

bool ServerEngine::sync_engine()
{
    sync_keypair();
    fetch_zparams();
    sync_merkle();
    sync_poolaccount();

    return true;
}

bool ServerEngine::sync_keypair()
{
    server_db = &(database::get_instance());
    mongocxx::options::find opt;
    std::vector<Keypair> keypair_temp;


    bsoncxx::document::view_or_value document{};
    if(!(server_db->query<Keypair>(keypair_temp, collection::key_pair, document, opt, "NOVALUE", false)))
    {
        LOG_DEBUG << "Keypair created";
        server_keypair.generate_keypair();
        //insert created keypair to database
        database::get_instance().add_request(server_keypair.serialize(), collector[collection::key_pair], "NOVALUE");
    }
    else{
        LOG_DEBUG << "Keypair recovered";
        server_keypair = keypair_temp[0];
    }

    return true;
}

bool ServerEngine::fetch_zparams()
{
    //fetch zk-snark parameters
    try {
        pzcashParams = ZCJoinSplit::Prepared("vkfile", "pkfile");
    }catch(...)
    {
        throw Exception("zk-snark parameters couldn't recover.");
        return false;
    }

    return true;
}


bool ServerEngine::sync_poolaccount()
{
    server_db = &(database::get_instance());
    mongocxx::options::find opt;
    std::vector<Account> poolvec;
    collection pool_account_type;
    if(test) {
        pool_account_type = collection::accounts_test;
    }
    else {
        pool_account_type = collection::accounts;
    }
    bsoncxx::document::view_or_value document{};

    // pool account definition
    if(!(server_db->query<Account>(poolvec, pool_account_type, document, opt, server_keypair.public_key(), true)))
    {
        LOG_DEBUG << "pool account  created";
        //insert created keypair to database
        database::get_instance().add_request(poolAccount.serialize(), collector[pool_account_type], server_keypair.public_key());
        LOG_DEBUG << "pool account balance: " << poolAccount.get_balance();
         }
    else{
        LOG_DEBUG << "pool account recovered";
        poolAccount = poolvec[0];
        LOG_DEBUG << "pool account balance: " << poolAccount.get_balance();

    }

    LOG_DEBUG << "pool account pk: " << poolAccount.get_paymentAddress()->get_pk().a_pk.ToString();


    return true;

}

bool ServerEngine::sync_merkle()
{
    // create or recover merkertree
    MerkleTree merkle_temp;
    std::vector<NoteCiphertext> note_temp;
    std::vector<NoteCiphertext> note_temp_reconstruct;

    if(!(query_note(note_temp, test)))
    {
        LOG_DEBUG << "sync_merkle: merkle created";
        //insert created merkle to database which is the default one
    }
    else{
        LOG_DEBUG << "sync_merkle: merkle recovered from database with height: " << note_temp[0].get_height();

        //reconstruct merkle tree from commitment list
        //TODO: figuring out some efficient way to save merkle tree on database
        for(int i = 1; i <= note_temp[0].get_height(); i++)
        {
            if(!query_note_one(i, note_temp_reconstruct, test)) {
                LOG_ERROR << "query note with height: " << i << " failed.";
                return false;
            }
            merkle_temp.update_merkle(note_temp_reconstruct[0].get_commitment());
            note_temp_reconstruct.clear();
        }
        merkleTree = merkle_temp;
        assert(merkleTree.get_merkle().root() == merkle_temp.get_merkle().root());
    }

    return true;

}


zmqpp::curve::keypair ServerEngine::get_keys()
{
    return server_keypair.get_keys();
}


bool ServerEngine::delete_keys()
{
    server_db = &(database::get_instance());
    bsoncxx::document::view_or_value filter{};

    int numbered_deleted;
    bool is_successful = database::get_instance().delete_request(collection::key_pair, filter, &numbered_deleted);
    LOG_DEBUG << "number of keypairs deleted: " <<  numbered_deleted;

    return is_successful;
}

Account& ServerEngine::get_pool_account()
{
    sync_poolaccount();
    return poolAccount;
}

bool ServerEngine::update_merkle(uint256 commitment)
{
    collection merkle_type;
    if(test)
        merkle_type = collection::merkle_tree_test;
    else
        merkle_type = collection::merkle_tree;


    bool final_result;
    bsoncxx::builder::basic::document update_info;

    bool result = merkleTree.update_merkle(commitment);
    LOG_WARNING << "merkle root to be update: " << merkleTree.get_merkle().root().ToString();
    LOG_WARNING << "serialized merkle to be update: " << merkleTree.serialize();


    //TODO : Update mechanism must be modified
//    bsoncxx::document::view_or_value document{};
//
//    if(result) {
//        update_info.append(bsoncxx::builder::basic::kvp("$set", [this](bsoncxx::builder::basic::sub_document temp){
//            temp.append(bsoncxx::builder::basic::kvp("merkletree", this->merkleTree.serialize()));
//        }));
//        final_result = database::get_instance().update(merkle_type, document, update_info.view(), "NOVALUE");
//    }else
//        final_result = false;

    // delete old merkle tree
    database::get_instance().delete_request(merkle_type, update_info.view());

    //add new merkle to database
    database::get_instance().add_request(merkleTree.serialize(), collector[merkle_type], "NOVALUE");

    return final_result;
}

bool ServerEngine::update_merkle(ZCIncrementalMerkleTree new_merkle) {

    LOG_DEBUG << "starting to update merkle tree.";
    bool final_result;
    // TODO: update mechanism must be modified
//    bsoncxx::builder::basic::document update_info;
    bsoncxx::builder::basic::document update_filter;
//    update_filter.append(bsoncxx::builder::basic::kvp("merkletree", MerkleTree(get_merkle()).serialize()));

    collection merkle_type;
    if(test)
        merkle_type = collection::merkle_tree_test;
    else
        merkle_type = collection::merkle_tree;


    try{

        merkleTree = MerkleTree(new_merkle);
        assert(merkleTree.get_merkle().root() == new_merkle.root());
        std::string temp_str = merkleTree.serialize();

       // TODO: update mechanism must be modified
//        update_info.append(bsoncxx::builder::basic::kvp("$set", [&temp_str](bsoncxx::builder::basic::sub_document temp){
//        temp.append(bsoncxx::builder::basic::kvp("merkletree", temp_str));
//          }));
//
//        final_result = database::get_instance().update(merkle_type, update_filter.view(), update_info.view(), "NOVALUE");
//        LOG_DEBUG << "merkle tree update result: " << final_result;

        //assert(get_merkle().root() == new_merkle.root());

        // delete old merkle tree
        database::get_instance().delete_request(merkle_type, update_filter.view());

        //add new merkle to database
        database::get_instance().add_request(merkleTree.serialize(), collector[merkle_type], "NOVALUE");

    }catch(...)
    {
        Exception("updating merkle tree failed.");
    }

    return final_result;
}




