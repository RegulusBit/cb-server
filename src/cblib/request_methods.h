//
// Created by reza on 4/17/18.
//

#ifndef TUTSERVER_REQUEST_METHODS_H
#define TUTSERVER_REQUEST_METHODS_H

#include <account.h>
#include <jsoncpp/json/value.h>
#include <primitives/transaction.h>
#include <boost/filesystem/path.hpp>
#include <boost/filesystem.hpp>
#include "zcash/Address.hpp"
#include "zcash/Note.hpp"
#include "zcash/Proof.hpp"
#include "zcash/JoinSplit.hpp"
#include "zcash/IncrementalMerkleTree.hpp"
#include "streams.h"
#include "util.h"
#include "zcash/NoteEncryption.hpp"

//add method to wallet repository
Json::Value get_registered_accounts(jsonParser parser, std::string user_id);
Json::Value purge_accounts(jsonParser parser, std::string user_id);
Json::Value register_account(jsonParser parser, std::string user_id);
Json::Value get_details(jsonParser parser, std::string user_id);
Json::Value transaction(jsonParser parser, std::string user_id);
Json::Value confidential_tx_mint(jsonParser parser, std::string user_id);
Json::Value confidential_tx_pour(jsonParser parser, std::string user_id);
Json::Value get_merkle_root(jsonParser parser, std::string user_id);
Json::Value get_notes(jsonParser parser, std::string user_id);

boost::array<unsigned char, ZC_MEMO_SIZE> get_memo_from_hex_string(std::string s);
std::string string_to_hex(const std::string& input);

// this function must call before any actions regarding to processing requests.
void methods_setup(void);




#endif //TUTSERVER_REQUEST_METHODS_H
