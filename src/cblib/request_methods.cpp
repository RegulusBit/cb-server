//
// Created by reza on 4/17/18.
//

#include <utilstrencodings.h>
#include "main.h"
#include "request_methods.h"


// utilities needed by methods TODO: create independent utility cpp and headers


boost::array<unsigned char, ZC_MEMO_SIZE> get_memo_from_hex_string(std::string s)
{
    boost::array<unsigned char, ZC_MEMO_SIZE> memo = {{0x00}};

    std::vector<unsigned char> rawMemo = ParseHex(s.c_str());

    // If ParseHex comes across a non-hex char, it will stop but still return results so far.
    size_t slen = s.length();
    if (slen % 2 != 0 || (slen > 0 && rawMemo.size() != slen / 2)) {
        throw Exception("Memo must be in hexadecimal format");
    }

    if (rawMemo.size() > ZC_MEMO_SIZE) {
        char* buffer;
        sprintf(buffer, "Memo size of %d is too big, maximum allowed is %d", (char*)rawMemo.size(), ZC_MEMO_SIZE);
        throw Exception(buffer);
    }

    // copy vector into boost array
    int lenMemo = rawMemo.size();
    for (int i = 0; i < ZC_MEMO_SIZE && i < lenMemo; i++) {
        memo[i] = rawMemo[i];
    }
    return memo;
}


std::string string_to_hex(const std::string& input)
{
    static const char* const lut = "0123456789ABCDEF";
    size_t len = input.length();

    std::string output;
    output.reserve(2 * len);
    for (size_t i = 0; i < len; ++i)
    {
        const unsigned char c = input[i];
        output.push_back(lut[c >> 4]);
        output.push_back(lut[c & 15]);
    }
    return output;
}


inline bool isInteger(const std::string & s)
{
    if(s.empty() || ((!isdigit(s[0])) && (s[0] != '-') && (s[0] != '+'))) return false ;

    char * p ;
    strtol(s.c_str(), &p, 10) ;

    return (*p == 0) ;
}

Json::Value response_creator(bool is_successful, Json::Value result)
{
    Json::Value response;

    if(is_successful)
        response["result"] = "successful";
    else
        response["result"] = "failed";

    response["response"] = result;

    return response;
}


Json::Value get_details(jsonParser parser, std::string user_id)
{
    LOG_INFO << "start getting details of account to client. GET_DETAILS requested.";
    Json::Value response;
    Json::Value result;
    std::vector<Account> result_vector;
    mongocxx::options::find opt;
    std::string pk;

    bsoncxx::builder::basic::document condition;

    if (!parser.get_parameter("pk", pk)) {
        LOG_WARNING << "account address is not provided.";
        result["description"] = "account address is not provided.";
        response = response_creator(false, result);
    } else {
        condition.append(bsoncxx::builder::basic::kvp("pk", pk));
        database::get_instance().query<Account>(result_vector, collection::accounts, condition.view(), opt, user_id, true);

        bool has_element = false;
        for (auto k: result_vector) {
            result.append(k.serialize());
            has_element = true;
        }

        if(has_element)
            response = response_creator(true, result);
        else
            response = response_creator(false, result);
    }
    return response;
}


Json::Value get_registered_accounts(jsonParser parser, std::string user_id)
{
    LOG_INFO << "start getting registered accounts to client. GET_REGISTERED_ACCOUNT requested.";
    Json::Value response;
    Json::Value result;
    std::vector<Account> result_vector;
    bsoncxx::document::view_or_value document{};
    mongocxx::options::find opt;

    database::get_instance().query<Account>(result_vector, collection::accounts, document, opt, user_id, true);

   for(auto k: result_vector)
    {
        result.append(k.serialize());
    }

    response = response_creator(true, result);
    return response;
}

Json::Value get_merkle_root(jsonParser parser, std::string user_id)
{
    LOG_INFO << "start getting merkle root to client. GET_MERKLE_ROOT requested.";
    Json::Value response;
    Json::Value result;
    std::vector<NoteCiphertext> result_vector;

    ServerEngine engine(false);

    if(query_note(result_vector, false)) {
        if(result_vector.size())
        {
            // in case of merkle tree with childs
            result["merkle_root"] = result_vector[0].get_root().ToString();
            response = response_creator(true, result);
        } else
        {
            result["description"] = "error in returning merkle tree";
            result["merkle_root"] = "NOVALUE";
            response = response_creator(false, result);
        }
    }else{

        // in case of empty merkle tree
        result["merkle_root"] = engine.get_merkle().root().ToString();
        response = response_creator(true, result);
    }

    return response;
}


Json::Value get_notes(jsonParser parser, std::string user_id)
{
    LOG_INFO << "start getting notes to client. GET_NOTES requested.";
    Json::Value response;
    Json::Value result;
    std::vector<NoteCiphertext> result_vector;
    std::string requested_note_height;
    std::string serialized_string;

    if (!parser.get_parameter("note_height", requested_note_height) || !isInteger(requested_note_height)) {
        LOG_WARNING << "requeting note height is not provided or is not valid.";
        result["description"] = "requeting note height is not provided or is not valid.";
        response = response_creator(false, result);
    } else
    {
        query_note(std::stoi(requested_note_height), result_vector, false);
        for(auto k : result_vector) {
            Json::Value element;
            k.get_string_serialization(serialized_string); //TODO: error handling
            LOG_DEBUG << "get notes result: " << serialized_string;
            result.append(serialized_string);
            serialized_string.clear();
        }

        response = response_creator(true, result);
    }


    return response;
}



Json::Value purge_accounts(jsonParser parser, std::string user_id)
{
    LOG_INFO << "start purging accounts. PURGE_ACCOUNTS requested.";
    Json::Value response;
    Json::Value result;
    bsoncxx::document::view_or_value document{};
    int* deleted_numbers = new int;
    bool is_successful = database::get_instance().delete_request(collection::accounts, document.view(), deleted_numbers);

    result["number_of_accounts_deleted"] = *deleted_numbers;
    response = response_creator(is_successful, result);

    return response;
}

Json::Value register_account(jsonParser parser, std::string user_id)
{
    LOG_INFO << "start registering accounts. REGISTER_ACCOUNT requested.";
    std::string balance;
    std::string pk;
    Json::Value response;
    Json::Value result;
    Account* temp;

    // there is no response in this method: TODO: add created account as response
    result["description"] = "";

    try {

        temp = new Account(parser.get_parameters());

        if(get_details(parser, user_id)["result"].asString() == "successful")
            {
                LOG_WARNING << "account already registered.";
                result["description"] = "account already registered.";
                response = response_creator(false, result);
            } else{

            if (database::get_instance().add_request(temp->serialize(), collector[collection::accounts], user_id)) {
                LOG_DEBUG << "requested account registered in database.";
                response = response_creator(true, result);
            } else {
                LOG_WARNING << "requested account cannot be added in database.";
                response = response_creator(false, result);
            }

            }

        if(temp)
            delete temp;

    }catch (...){
        LOG_ERROR << "registering failed without known result.";
        response = response_creator(false, result);
    }

    return response;
}

bool simpleTXverification(Json::Value& response, std::string tx_value, std::string from_pk, std::string to_pk, std::string user_id)
{
    bool state;
    Json::Value result;
    std::vector<Account> result_vector_from;
    std::vector<Account> result_vector_to;
    bsoncxx::builder::basic::document condition;
    bsoncxx::builder::basic::document update;
    mongocxx::options::find opt;

    LOG_DEBUG << "transaction value: " << std::stoi(tx_value);
    condition.append(bsoncxx::builder::basic::kvp("pk", from_pk));
    database::get_instance().query<Account>(result_vector_from, collection::accounts, condition.view(), opt, user_id, true);

    condition.clear();
    condition.append(bsoncxx::builder::basic::kvp("pk", to_pk));
    database::get_instance().query<Account>(result_vector_to, collection::accounts, condition.view(), opt, user_id, false);


    if(!result_vector_from.size())
    {
        LOG_WARNING << "client are not authorized to perform this transaction.";
        LOG_WARNING << "requested payee public key: " << from_pk;
        result["description"] = "you are not authorized to perform this transaction.";
        response = response_creator(false, result);
        state = false;
    } else if(!result_vector_to.size())
    {
        LOG_WARNING << "destination account is not registered.";
        LOG_WARNING << "requested destination public key: " << to_pk;
        result["description"] = "destination account is not registered.";
        response = response_creator(false, result);
        state = false;
    }
    else
    {
        // in default mode there is exactly one account with specific pk in database
        if(result_vector_from[0].get_balance() < std::stoi(tx_value))
        {
            LOG_WARNING << "clients deposit value is not enough.";
            result["description"] = "your deposit value is not enough.";
            response = response_creator(false, result);
            state = false;
        } else
        {

            // update database based on TX information
            condition.clear();
            condition.append(bsoncxx::builder::basic::kvp("pk", from_pk));

            update.append(bsoncxx::builder::basic::kvp("$set", [&result_vector_from, tx_value](bsoncxx::builder::basic::sub_document temp){
                temp.append(bsoncxx::builder::basic::kvp("balance", (result_vector_from[0].get_balance() - std::stoi(tx_value))));
            }));




            if(database::get_instance().update(collection::accounts, condition.view(), update.view(), user_id)) {

                condition.clear();
                update.clear();
                condition.append(bsoncxx::builder::basic::kvp("pk", to_pk));

                update.append(bsoncxx::builder::basic::kvp("$set", [&result_vector_to, tx_value](bsoncxx::builder::basic::sub_document temp){
                    temp.append(bsoncxx::builder::basic::kvp("balance", (result_vector_to[0].get_balance() + std::stoi(tx_value))));
                }));

                if(database::get_instance().update(collection::accounts, condition.view(), update.view(), user_id)) {
                    LOG_DEBUG << "transaction performed successfully.";
                    result["description"] = "transaction performed successfully.";
                    response = response_creator(true, result);
                    state = true;
                }else{
                    LOG_ERROR << "transaction failed due to database error.";
                    result["description"] = "transaction failed due to database error.";
                    response = response_creator(false, result);
                    state = false;
                }
            } else
            {
                LOG_ERROR << "transaction failed due to database error.";
                result["description"] = "transaction failed due to database error.";
                response = response_creator(false, result);
                state = false;
            }
        }
    }

    return state;
}


Json::Value transaction(jsonParser parser, std::string user_id)
{
    LOG_INFO << "start performing transaction. TRANSACTION requested.";
    std::string tx_value;
    std::string from_pk;
    std::string to_pk;
    Json::Value response;
    Json::Value result;


    // there is no response in this method: TODO: add created account as response
    result["description"] = "";

    try {

        // processing the transaction
        if (!parser.get_parameter("tx_value", tx_value) || !isInteger(tx_value) || !parser.get_parameter("from_pk", from_pk) || !parser.get_parameter("to_pk", to_pk)) {
            LOG_WARNING << "transaction addresses or value is not provided.";
            result["description"] = "transaction addresses or value is not provided.";
            response = response_creator(false, result);
        } else {

            simpleTXverification(response, tx_value, from_pk, to_pk, user_id);
        }

    }catch (...){
        LOG_ERROR << "registering failed without known result.";
        response = response_creator(false, result);
    }

    return response;
}


Json::Value confidential_tx_mint(jsonParser parser, std::string user_id)
{
    bool merkle_is_valid = true;
    bool nullifier_is_valid = true;

    LOG_INFO << "start performing confidential transaction. CONFIDENTIAL_TX_MINT requested.";
    Json::Value response;
    Json::Value result;
    ServerEngine engine(false);
    std::vector<NoteCiphertext> dummy;

    //TODO: save zcash params in database
    // get public parameters from engine
    ZCIncrementalMerkleTree merkleTree = engine.get_merkle();
    ZCJoinSplit *pzcashParams = engine.get_zkparams();

    assert(pzcashParams != nullptr);
    auto verifier = libzcash::ProofVerifier::Strict();

    // recover transaction data from request
    CMutableTransaction mtx;
    mtx.nVersion = 2;
    CTransaction tx(mtx);
    std::string tx_string;
    parser.get_parameter("tx", tx_string);
    std::istringstream received_tx(tx_string);
    tx.Unserialize(received_tx, 1, 2);

    // in this point we have requested transaction in form of CTransaction

    // check if provided merkle root is in the merkle roots set in database
    // and the nullifier set should not be in the set
    for(auto k : tx.vjoinsplit) {
        // merkle root set
        if (!query_note(k.anchor, dummy, false)) {
            if (k.anchor != engine.get_merkle().root()) {
                LOG_ERROR << "sent merkle is not valid.";
                merkle_is_valid = false;
                break;
            }
        }

        //nullifier set
        for (int i = 0; i < 2; i++) {
            if(query_nullifier(k.nullifiers[i], dummy, false))
            {
                nullifier_is_valid = false;
                break;
            }
        }
    }

    CValidationState state;
    CValidationState state_2;

    if(merkle_is_valid && nullifier_is_valid)
    {
        ContextualCheckTransaction(tx, state);
        LOG_DEBUG << "contextual validation state: " << state.IsValid();
        result["contextual validation state: "] = state.IsValid();

        //CheckTransaction(tx, state_2, verifier);

        LOG_DEBUG << "ZKP validation state: " << state_2.IsValid();
        result["ZKP validation state: "] = state_2.IsValid();
    }


    bool state_final = (/*state.IsValid()*/ true && state_2.IsValid());
    bool simple_tx_is_valid = false;
    bool adding_note = true;

    if(merkle_is_valid && nullifier_is_valid && state_final && tx.vin.size() == 2)
    {

        if(simpleTXverification(response, std::to_string(tx.vjoinsplit[0].vpub_old) ,tx.vin[0].prevout.hash.ToString(),
                                engine.get_pool_account().get_paymentAddress()->get_pk().a_pk.ToString() , user_id))
        {
            if(simpleTXverification(response, std::to_string(tx.vjoinsplit[0].vpub_new),
                                 engine.get_pool_account().get_paymentAddress()->get_pk().a_pk.ToString(), tx.vin[1].prevout.hash.ToString(), engine.get_keys().public_key))
            {
                simple_tx_is_valid = true;

                adding_note = add_note(tx.vjoinsplit[0], tx.joinSplitPubKey, false);
            }
        }
    }
    result["tx_check"] = state_final;
    result["transparent_tx"] = simple_tx_is_valid;
    result["adding_note"] = adding_note;
    result["merkle_root_validation"] = merkle_is_valid;
    result["nullifier_validation"] = nullifier_is_valid;

    response = response_creator((state_final && merkle_is_valid && nullifier_is_valid && simple_tx_is_valid && adding_note ), result);

    return response;
}



Json::Value confidential_tx_pour(jsonParser parser, std::string user_id)
{
    bool merkle_is_valid = true;
    bool nullifier_is_valid = true;

    LOG_INFO << "start performing confidential transaction. CONFIDENTIAL_TX_POUR requested.";
    Json::Value response;
    Json::Value result;
    ServerEngine engine(false);
    std::vector<NoteCiphertext> dummy;

    //TODO: save zcash params in database
    // get public parameters from engine
    ZCIncrementalMerkleTree merkleTree = engine.get_merkle();
    ZCJoinSplit *pzcashParams = engine.get_zkparams();

    assert(pzcashParams != nullptr);
    auto verifier = libzcash::ProofVerifier::Strict();

    // recover transaction data from request
    CMutableTransaction mtx;
    mtx.nVersion = 2;
    CTransaction tx(mtx);
    std::string tx_string;
    parser.get_parameter("tx", tx_string);
    std::istringstream received_tx(tx_string);
    tx.Unserialize(received_tx, 1, 2);

    // in this point we have requested transaction in form of CTransaction

    // check if provided merkle root is in the merkle roots set in database
    // and the nullifier set should not be in the set
    for(auto k : tx.vjoinsplit) {
        // merkle root set
        if (!query_note(k.anchor, dummy, false)) {
            if (k.anchor != engine.get_merkle().root()) {
                LOG_ERROR << "sent merkle is not valid.";
                merkle_is_valid = false;
                break;
            }
        }

        //nullifier set
        for (int i = 0; i < 2; i++) {
            if(query_nullifier(k.nullifiers[i], dummy, false))
            {
                nullifier_is_valid = false;
                LOG_ERROR << "nullifier with nullifier: " << k.nullifiers[i].ToString() << " is not valid.";
                break;
            }
        }
    }

    CValidationState state;
    CValidationState state_2;

    if(merkle_is_valid && nullifier_is_valid)
    {
        ContextualCheckTransaction(tx, state);
        LOG_DEBUG << "contextual validation state: " << state.IsValid();
        result["contextual validation state: "] = state.IsValid();

        //CheckTransaction(tx, state_2, verifier);

        LOG_DEBUG << "ZKP validation state: " << state_2.IsValid();
        result["ZKP validation state: "] = state_2.IsValid();
    }


    bool state_final = (/*state.IsValid()*/ true && state_2.IsValid());
    bool simple_tx_is_valid = false;
    bool adding_note = true;

    if(merkle_is_valid && nullifier_is_valid && state_final && tx.vin.size() == 2)
    {
        CAmount vpub_old = 0;
        CAmount vpub_new = 0;

        int counter = 0;
        for(auto joinsplit : tx.vjoinsplit)
        {
            counter++;
            vpub_old += joinsplit.vpub_old;
            vpub_new += joinsplit.vpub_new;
        }

        LOG_DEBUG << "total joinsplits received: " << counter;

        if(simpleTXverification(response, std::to_string(vpub_old) ,tx.vin[0].prevout.hash.ToString(),
                                engine.get_pool_account().get_paymentAddress()->get_pk().a_pk.ToString() , user_id))
        {
            if(simpleTXverification(response, std::to_string(vpub_new),
                                    engine.get_pool_account().get_paymentAddress()->get_pk().a_pk.ToString(), tx.vin[1].prevout.hash.ToString(), engine.get_keys().public_key))
            {
                simple_tx_is_valid = true;

                for(auto joinsplit : tx.vjoinsplit)
                {
                    adding_note = add_note(joinsplit, tx.joinSplitPubKey, false);
                }

            }
        }
    }
    result["tx_check"] = state_final;
    result["transparent_tx"] = simple_tx_is_valid;
    result["adding_note"] = adding_note;
    result["merkle_root_validation"] = merkle_is_valid;
    result["nullifier_validation"] = nullifier_is_valid;

    response = response_creator((state_final && merkle_is_valid && nullifier_is_valid && simple_tx_is_valid && adding_note ), result);

    return response;
}


void methods_setup(void)
{
    Repository::get_instance().add_to_repository("get_registered_accounts", get_registered_accounts);
    Repository::get_instance().add_to_repository("purge_accounts", purge_accounts);
    Repository::get_instance().add_to_repository("register_account", register_account);
    Repository::get_instance().add_to_repository("get_details", get_details);
    Repository::get_instance().add_to_repository("transaction", transaction);
    Repository::get_instance().add_to_repository("confidential_tx_mint", confidential_tx_mint);
    Repository::get_instance().add_to_repository("confidential_tx_pour", confidential_tx_pour);
    Repository::get_instance().add_to_repository("get_merkle_root", get_merkle_root);
    Repository::get_instance().add_to_repository("get_notes", get_notes);

}