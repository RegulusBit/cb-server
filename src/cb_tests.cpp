//
// Created by reza on 4/13/18.
//

#include <src/cblib/request_methods.h>
#include "cb_tests.h"
#include <bsoncxx/builder/basic/impl.hpp>
#include <utilstrencodings.h>
#include <src/cblib/note.h>
#include <primitives/transaction.h>
#include "zcash/Address.hpp"
#include "zcash/Note.hpp"
#include "zcash/Proof.hpp"
#include "zcash/JoinSplit.hpp"
#include "zcash/IncrementalMerkleTree.hpp"
#include "src/cblib/note.h"
#include "zcash/NoteEncryption.hpp"
#include "bsoncxx/builder/basic/kvp.hpp"
#include "src/cblib/request_methods.h"


void cb_address_tests(void)
{
    LOG_DEBUG << "CBPaymentAddress tests begins.";

    // create dummy spending key
    CBPaymentAddress payAddr;
    LOG_DEBUG << "payment address created with spending key: " << payAddr.get_sk().inner().ToString();
    LOG_DEBUG << "created payment address has pk: " << payAddr.get_pk().a_pk.ToString();

    // turn dummy_sk to string format for saving in database
    std::string dummy_sk_string = payAddr.get_pk().a_pk.ToString();

    // create another payment address from string
    CBPaymentAddress dummy_payment_second(dummy_sk_string);
    LOG_DEBUG << "created dummy payment address has pk: " << dummy_payment_second.get_pk().a_pk.ToString();
    assert(payAddr.get_pk().a_pk == dummy_payment_second.get_pk().a_pk);

    //serialize tests
    Json::FastWriter fst;
    LOG_DEBUG << "payment address serialized: " << fst.write(payAddr.serialize());


}



void cb_db_tests(void)
{
    // filter specification
    bsoncxx::document::view_or_value filter{};
    collection result_type = collection::accounts_test;
    std::vector<Account> result_vector;
    Json::StyledWriter wrt;

    // accessing test db
    mongocxx::database& test_db = database::get_instance().get_db();
    mongocxx::collection coll = test_db.collection(collector[result_type]);

    // drop all collection documents
    coll.drop();

    // extract every document
    mongocxx::cursor curser = coll.find(filter);
    LOG_DEBUG << "database successfully accessed.";

    bool has_element = false;
    for(auto doc : curser)
    {
        Account temp(bsoncxx::to_json(doc));
        result_vector.push_back(temp);
        LOG_DEBUG << "recovered account: " << wrt.write(temp.serialize());
        has_element = true;
    }

    assert(!has_element);
    LOG_DEBUG << "accounts_test has elements: " << has_element;

    // add document to database
    // create dummy account with balance value of 50
    Account test_account(50);

    //insert dummy account to database
    database::get_instance().add_request(test_account.serialize(), collector[result_type], "test");

    // extract every document
    curser = coll.find(filter);
    LOG_DEBUG << "database successfully accessed.";

    has_element = false;
    for(auto doc : curser)
    {
        Account temp(bsoncxx::to_json(doc));
        result_vector.push_back(temp);
        LOG_DEBUG << "recovered account: " << wrt.write(temp.serialize());
        has_element = true;
    }

    assert(has_element);
    LOG_DEBUG << "after addition has elements: " << has_element;



    //non-blinded query
    // dummy account with value of 25 as a query target
    Account test_account2(25);
    database::get_instance().add_request(test_account2.serialize(), collector[result_type], "test");

//    TODO: condition in stream mode doesn't work
//    bsoncxx::builder::basic::document condition;
//    condition.append(bsoncxx::builder::basic::kvp("payment_address",
//                                                  [](bsoncxx::builder::basic::document sa)
//                                                  {sa.append(bsoncxx::builder::basic::kvp("pk","pk"));}));

    bsoncxx::builder::basic::document condition;
    condition.append(bsoncxx::builder::basic::kvp("balance", 25));

    curser = coll.find(condition.view());

    has_element = false;
    for(auto doc : curser)
    {
        Account temp(bsoncxx::to_json(doc));
        result_vector.push_back(temp);
        LOG_DEBUG << "query result: " << bsoncxx::to_json(doc);
        LOG_DEBUG << "recovered account: " << wrt.write(temp.serialize());
        has_element = true;
    }

    assert(has_element);
    LOG_DEBUG << "query has elements: " << has_element;


    // updating test

    bsoncxx::builder::basic::document update;
    update.append(bsoncxx::builder::basic::kvp("$set", [](bsoncxx::builder::basic::sub_document temp){
        temp.append(bsoncxx::builder::basic::kvp("balance", 40));
    }));

    assert(database::get_instance().update(collection::accounts_test, condition.view(), update.view(), "NOVALUE"));


    // deleting tests:
    LOG_DEBUG << "deleting tests starts.deleting documents of testDB";

    condition.clear();
    int* deleted_number = new int;
    assert(database::get_instance().delete_request(result_type, condition.view(), deleted_number));
    LOG_DEBUG << "number of deleted documents: " << *deleted_number;
    delete deleted_number;
    // free the test database

    // extract every document
    curser = coll.find(filter);
    LOG_DEBUG << "database successfully accessed.";

    has_element = false;
    for(auto doc : curser)
    {
        Account temp(bsoncxx::to_json(doc));
        result_vector.push_back(temp);
        LOG_DEBUG << "recovered account: " << wrt.write(temp.serialize());
        has_element = true;
    }

    assert(!has_element);
    LOG_DEBUG << "accounts_test has elements: " << has_element;


    coll.drop();
}

Json::Value test_method(jsonParser parser, std::string user_id)
{
    LOG_DEBUG << "test_method reached out.";
    Json::Value response;
    response["result"] = "successful";
    return response;
}

void cb_request_tests(void)
{

    // some useful tools
    Json::Value request;
    Json::Value response;
    Json::FastWriter fst;
    Json::Reader rdr;

    //TODO: list of request test cases should update with newer added methods
    //create dummy request
    request["Method"] = "get_accounts";
    jsonParser parser(fst.write(request));

    // insert and get dummy method type for testing repository
    LOG_DEBUG << "test method created.";
    Repository::get_instance().add_to_repository("test_method", &test_method);
    Repository::get_instance().get_from_repository("test_method")(parser, "NOVALUE");

    // setup phase to create method repository
    methods_setup();

    // test get_accounts method
    request_ptr method_ptr = Repository::get_instance().get_from_repository(parser.get_header());

    if(method_ptr) {
        LOG_DEBUG << "get_accounts method found.";
    } else
        LOG_DEBUG << "get accounts method not found.";

}


void cb_note_ciphertext_tests()
{

    collection result_type = collection::notes_test;
    // pp: public parameters used in the network
    // construct a merkle tree
    ZCIncrementalMerkleTree merkleTree;
    ZCJoinSplit *pzcashParams = NULL;
    pzcashParams = ZCJoinSplit::Prepared("vkfile", "pkfile");

    assert(pzcashParams != nullptr);

    //Address components
    libzcash::SpendingKey recipient_key = libzcash::SpendingKey::random();
    libzcash::PaymentAddress payment_address = recipient_key.address();

    auto sk_enc = ZCNoteEncryption::generate_privkey(recipient_key);
    auto pk_enc = ZCNoteEncryption::generate_pubkey(sk_enc);

    LOG_DEBUG << "recipient_key(test): " << recipient_key.inner().ToString();
    LOG_DEBUG << "payment_address(test): " << recipient_key.address().a_pk.ToString();
    LOG_DEBUG << "encryption_private_key(test): " << recipient_key.viewing_key().sk_enc.ToString();
    LOG_DEBUG << "encryption_public_key(test): " << recipient_key.viewing_key().sk_enc.pk_enc().ToString();

    std::string memo = "TEST MEMO";
    auto result = get_memo_from_hex_string(string_to_hex(memo));
    LOG_DEBUG << "memo result: " << result.data() << std::endl;


    // note is the basecoin definition. value of this coin is 100
    // including:
    //              value
    //              commitment
    //              serial number
    //              owner address
    libzcash::Note note(payment_address.a_pk, 100, uint256(), uint256());

    // commitment from coin
    uint256 commitment = note.cm();
    std::cout << "original commitment: " << commitment.ToString() << std::endl;


    // insert commitment into the merkle tree
    merkleTree.append(commitment);

    // compute the merkle root we will be working with
    uint256 rt = merkleTree.root();


    libzcash::NotePlaintext plainNote(note, result);
    assert(plainNote.note(payment_address).cm() == note.cm());

    auto h_sig = uint256();
    ZCNoteEncryption encrypter = ZCNoteEncryption(h_sig); // h_sig is required

    auto ciphertext = plainNote.encrypt(encrypter, pk_enc);

    Json::Value cipher;
    Json::FastWriter fst;
    std::wstring temp_string;
    std::vector<NoteCiphertext> result_vector;
    bsoncxx::builder::basic::document condition;
    mongocxx::options::find opt;

//    auto nullifier = uint256();
//    NoteCiphertext ciphernote(ciphertext, encrypter.get_epk(), nullifier);
//    std::string serialized_note = ciphernote.serialize(h_sig, note.cm(),nullifier, merkleTree, 1);
//    std::string serialized_note_2 = ciphernote.serialize(h_sig, note.cm(),nullifier, merkleTree, 2);
//
//    database::get_instance().delete_request(collection::notes_test, condition.view());
//    database::get_instance().add_request(serialized_note, collector[result_type], "TEST");
//    database::get_instance().add_request(serialized_note_2, collector[result_type], "TEST");
//    assert(database::get_instance().query<NoteCiphertext>(result_vector, collection::notes_test, condition.view(),opt , "TEST", false));
//
//    if(!result_vector.size())
//        LOG_ERROR << "ciphertext note did not recovered.";
//
//    condition.append(bsoncxx::builder::basic::kvp("note_height", -1));
//
//    opt.sort(condition.view());
//    bsoncxx::builder::basic::document cond;
//    std::vector<NoteCiphertext> result_vec;
//    database::get_instance().query<NoteCiphertext>(result_vec, collection::notes_test, cond.view(), opt, "TEST", false);
//
//    ZCNoteDecryption::Ciphertext cipher_dest = result_vec[0].get_ciphertext(); // there is one result in default
//
//
//
//    libzcash::NotePlaintext plainNote_result;
//    ZCNoteDecryption decrypter(sk_enc);
//    assert(result_vec[0].get_ephemeral_key() == encrypter.get_epk());
//    assert(result_vec[0].get_h_sig() == h_sig);
//    plainNote_result = libzcash::NotePlaintext::decrypt(decrypter,cipher_dest , result_vec[0].get_ephemeral_key(), result_vec[0].get_h_sig(), 0);
//
//    LOG_DEBUG << "decryption result(memo): " << plainNote_result.memo.data() << std::endl;
//
//
//
//    //query_note and add_note tests
//    condition.clear();
//    database::get_instance().delete_request(collection::notes_test, condition.view());
//    database::get_instance().delete_request(collection::merkle_tree_test, condition.view());
//    database::get_instance().delete_request(collection::accounts_test, condition.view());
//
//    // start tests here
//    LOG_WARNING << "starting query_note and add_note methodds tests.";
//    ServerEngine testEngine(true);
//
//    // Set up a JoinSplit description
//    uint256 ephemeralKey;
//    uint256 randomSeed;
//    uint64_t vpub_old = 100;
//    uint64_t vpub_new = 0;
//    uint256 pubKeyHash = libzcash::random_uint256();
//    boost::array<uint256, 2> macs;
//    boost::array<uint256, 2> nullifiers;
//    boost::array<uint256, 2> commitments;
//    rt = testEngine.get_merkle().root();
//    boost::array<ZCNoteEncryption::Ciphertext, 2> ciphertexts;
//    libzcash::ZCProof proof;
//
//    boost::array<libzcash::JSInput, ZC_NUM_JS_INPUTS> inputs = {
//            libzcash::JSInput(), // dummy
//            libzcash::JSInput() // dummy input of zero value
//    };
//    boost::array<libzcash::JSOutput, ZC_NUM_JS_OUTPUTS> outputs = {
//            libzcash::JSOutput(payment_address, 50),
//            libzcash::JSOutput(payment_address, 50)
//    };
//    boost::array<libzcash::Note, 2> output_notes;
//
//
//    JSDescription jsec(*testEngine.get_zkparams(), pubKeyHash, rt, inputs, outputs, vpub_old, vpub_new, false);
//    // two new cipher notes must be appended to database
//    LOG_WARNING << "what merkle is before addition: " << testEngine.get_merkle().root().ToString();
//
//    ZCIncrementalMerkleTree merkle_add_note = testEngine.get_merkle();
//    add_note(jsec, pubKeyHash, true);
//
//    merkle_add_note.append(jsec.commitments[0]);
//    merkle_add_note.append(jsec.commitments[1]);
//
//    ServerEngine secondtest(true);
//
//    LOG_WARNING << "what merkle sould lool like: " << merkle_add_note.root().ToString();
//    LOG_WARNING << "what merkle is: " << secondtest.get_merkle().root().ToString();
////    assert(merkle_add_note.root() == testEngine.get_merkle().root());
//
//
//    result_vector.clear();
//    query_note(result_vector, true);
//    assert(result_vector.size());
//    assert(result_vector[0].get_height() == 2);
//    LOG_DEBUG << "recovered note height is: " << result_vector[0].get_height();
//
//    result_vector.clear();
//    query_note(1, result_vector, true);
//    assert(result_vector[0].get_ciphertext_string() == NoteCiphertext(jsec.ciphertexts[1]).get_ciphertext_string());
//
//    result_vector.clear();
//    query_note(0, result_vector, true);
//    assert(result_vector.size() == 2);
//
////    result_vector.clear();
////    query_note(testEngine.get_merkle().root(), result_vector, true);
////    assert(result_vector.size());
////    assert(result_vector[0].get_height() == 2);
//
//
//    add_note(jsec, true);
//    result_vector.clear();
//    query_note(0, result_vector, true);
//    assert(result_vector.size() == 4);

}

void cb_confidential_transaction_tests()
{
    collection account_result_type = collection::accounts_test;
    bsoncxx::document::view_or_value filter;

    // pp: public parameters used in the network
    // construct a merkle tree
    ServerEngine testEngine(true);

    assert(testEngine.get_zkparams() != nullptr);

    //sender Address components
    // create dummy account with balance value of 50
    Account test_account(500);

    libzcash::SpendingKey recipient_key = test_account.get_paymentAddress()->get_sk();
    libzcash::PaymentAddress payment_address = recipient_key.address();
    auto sk_enc = ZCNoteEncryption::generate_privkey(recipient_key);
    auto pk_enc = ZCNoteEncryption::generate_pubkey(sk_enc);

    LOG_DEBUG << "recipient_key(test): " << recipient_key.inner().ToString();
    LOG_DEBUG << "payment_address(test): " << recipient_key.address().a_pk.ToString();
    LOG_DEBUG << "encryption_private_key(test): " << recipient_key.viewing_key().sk_enc.ToString();
    LOG_DEBUG << "encryption_public_key(test): " << recipient_key.viewing_key().sk_enc.pk_enc().ToString();

    //sender initial balance configurations
    //add document to database
    database::get_instance().delete_request(collection::accounts_test, filter);
    database::get_instance().add_request(test_account.serialize(), collector[account_result_type], "TEST");


    //recipient address components
    libzcash::SpendingKey recipient_key_rec = libzcash::SpendingKey::random();
    libzcash::PaymentAddress payment_address_rec = recipient_key_rec.address();
    auto sk_enc_rec = ZCNoteEncryption::generate_privkey(recipient_key_rec);
    auto pk_enc_rec = ZCNoteEncryption::generate_pubkey(sk_enc_rec);




    // create transaction
    std::string memo = recipient_key.address().a_pk.ToString();
    auto result = get_memo_from_hex_string(string_to_hex(memo));
    LOG_DEBUG << "memo result: " << result.data() << std::endl;


    // note is the basecoin definition. value of this coin is 100
    // including:
    //              value
    //              commitment
    //              serial number
    //              owner address
    libzcash::Note note(payment_address.a_pk, 100, uint256(), uint256());
    libzcash::NotePlaintext plainNote(note, result);
    assert(plainNote.note(payment_address).cm() == note.cm());


    auto h_sig = uint256();
    ZCNoteEncryption encrypter = ZCNoteEncryption(h_sig); // h_sig is required

    auto ciphertext = plainNote.encrypt(encrypter, pk_enc);

    // insert commitment into the merkle tree
    auto commitment = note.cm();

    // compute the merkle root we will be working with
    uint256 rt = testEngine.get_merkle().root();


    // Set up a JoinSplit description
    uint256 ephemeralKey;
    uint256 randomSeed;
    uint64_t vpub_old = 100;
    uint64_t vpub_new = 0;
    uint256 pubKeyHash = libzcash::random_uint256();
    boost::array<uint256, 2> macs;
    boost::array<uint256, 2> nullifiers;
    boost::array<uint256, 2> commitments;
    rt = testEngine.get_merkle().root();
    boost::array<ZCNoteEncryption::Ciphertext, 2> ciphertexts;
    libzcash::ZCProof proof;


    boost::array<libzcash::JSInput, ZC_NUM_JS_INPUTS> inputs = {
            libzcash::JSInput(), // dummy
            libzcash::JSInput() // dummy input of zero value
    };
    boost::array<libzcash::JSOutput, ZC_NUM_JS_OUTPUTS> outputs = {
            libzcash::JSOutput(payment_address, 100),
            libzcash::JSOutput()
    };


    boost::array<libzcash::Note, 2> output_notes;

    ciphertexts[0] = ciphertext;


    JSDescription jsec(*testEngine.get_zkparams(), pubKeyHash, rt, inputs, outputs, vpub_old, vpub_new, false);
    jsec.ciphertexts = ciphertexts;


    auto verifier = libzcash::ProofVerifier::Strict();

}


void cb_joinsplit_tests()
{
    collection account_result_type = collection::accounts_test;
    bsoncxx::document::view_or_value filter;

    // pp: public parameters used in the network
    // construct a merkle tree
    ServerEngine testEngine(true);

    assert(testEngine.get_zkparams() != nullptr);

    //sender Address components
    // create dummy account with balance value of 50
    Account test_account(500);

    libzcash::SpendingKey recipient_key = test_account.get_paymentAddress()->get_sk();
    libzcash::PaymentAddress payment_address = recipient_key.address();
    auto sk_enc = ZCNoteEncryption::generate_privkey(recipient_key);
    auto pk_enc = ZCNoteEncryption::generate_pubkey(sk_enc);

    auto tree = testEngine.get_merkle();

    // Set up a JoinSplit description
    uint256 ephemeralKey;
    uint256 randomSeed;
    uint64_t vpub_old = 10;
    uint64_t vpub_new = 0;
    uint256 pubKeyHash = libzcash::random_uint256();
    boost::array<uint256, 2> macs;
    boost::array<uint256, 2> nullifiers;
    boost::array<uint256, 2> commitments;
    uint256 rt = tree.root();
    boost::array<ZCNoteEncryption::Ciphertext, 2> ciphertexts;
    libzcash::ZCProof proof;

    {
        boost::array<libzcash::JSInput, 2> inputs = {
                libzcash::JSInput(), // dummy input
                libzcash::JSInput() // dummy input
        };

        boost::array<libzcash::JSOutput, 2> outputs = {
                libzcash::JSOutput(payment_address, 10),
                libzcash::JSOutput() // dummy output
        };

        boost::array<libzcash::Note, 2> output_notes;

        // Perform the proof
        proof = testEngine.get_zkparams()->prove(
                inputs,
                outputs,
                output_notes,
                ciphertexts,
                ephemeralKey,
                pubKeyHash,
                randomSeed,
                macs,
                nullifiers,
                commitments,
                vpub_old,
                vpub_new,
                rt
        );

        LOG_DEBUG << "ephemeral key: " << ephemeralKey.ToString();
        LOG_DEBUG << "random seed: " << randomSeed.ToString();
        LOG_DEBUG << "pubkey hash: " << pubKeyHash.ToString();
    }
}

JSDescription create_js(uint256 rt)
{

    libzcash::SpendingKey recipient_key = libzcash::SpendingKey::random();
    libzcash::PaymentAddress payment_address = recipient_key.address();

    // Set up a JoinSplit description
    uint256 ephemeralKey;
    uint256 randomSeed;
    uint64_t vpub_old = 100;
    uint64_t vpub_new = 0;
    uint256 pubKeyHash = libzcash::random_uint256();
    boost::array<uint256, 2> macs;
    boost::array<uint256, 2> nullifiers;
    boost::array<uint256, 2> commitments;
    boost::array<ZCNoteEncryption::Ciphertext, 2> ciphertexts;
    libzcash::ZCProof proof;


    boost::array<libzcash::JSInput, ZC_NUM_JS_INPUTS> inputs = {
            libzcash::JSInput(), // dummy
            libzcash::JSInput() // dummy input of zero value
    };
    boost::array<libzcash::JSOutput, ZC_NUM_JS_OUTPUTS> outputs = {
            libzcash::JSOutput(payment_address, 100),
            libzcash::JSOutput()
    };


    boost::array<libzcash::Note, 2> output_notes;

    ServerEngine testEngine(true);

    JSDescription jsec(*testEngine.get_zkparams(), pubKeyHash, rt, inputs, outputs, vpub_old, vpub_new, false);

    return jsec;
}


void cb_merkle_tests(void)
{
    bsoncxx::document::view_or_value condition;
    database::get_instance().delete_request(collection::notes_test, condition);
    LOG_DEBUG << "merkle tree tests starts.";

    ZCIncrementalMerkleTree test;
    uint256 pubkeyhash;
    std::vector<NoteCiphertext> test_vector;
    ServerEngine testEngine(true);

    assert(testEngine.get_merkle().root() == test.root());

    LOG_WARNING << "initial merkle result: " << test.root().ToString();

    JSDescription js = create_js(test.root());
    test.append(js.commitments[0]);
    test.append(js.commitments[1]);
    add_note(js,pubkeyhash, true);

    query_note(test_vector, true);
    assert(test_vector[0].get_height() == 2);
    assert(test_vector[0].get_commitment() == js.commitments[1]);

    uint256 rt = testEngine.get_merkle().root();

    LOG_WARNING << "local merkle result: " << test.root().ToString();
    LOG_WARNING << "engine merkle result: " << rt.ToString();


    assert( rt == test.root());

    test.append(js.commitments[0]);
    test.append(js.commitments[1]);
    add_note(js,pubkeyhash, true);

    rt = testEngine.get_merkle().root();
    assert( rt == test.root());

    ServerEngine testEngine_2(true);

    assert(testEngine.get_merkle().root() == testEngine_2.get_merkle().root());

    //TODO
//    ZCIncrementalMerkleTree test;
//    MerkleTree to_be_tested = MerkleTree(MerkleTree(test).serialize());
//    assert(test.root() == to_be_tested.get_merkle().root());
//
//
//
//    MerkleTree to_be_tested_2 = MerkleTree(MerkleTree(test).serialize());
//
//    LOG_WARNING << "LOCAL MERKLE: " << test.root().ToString();
//    LOG_WARNING << "TESTED MERKLE: " << to_be_tested_2.get_merkle().root().ToString();


    //TODO
//    ServerEngine engine(false);
//    ZCIncrementalMerkleTree test_merkle;
//
//    LOG_WARNING << "initial local merkle tree: " << test_merkle.root().ToString();
//    LOG_WARNING << "initial test merkle tree: " << engine.get_merkle().root().ToString();
//
//    test_merkle.append(js.commitments[0]);
//    LOG_WARNING << "local merkle after appending note: " << test_merkle.root().ToString();
//
//    engine.update_merkle(js.commitments[0]);
//    LOG_WARNING << "test merkle tree after appenfing note: " << engine.get_merkle().root().ToString();
}


void start_tests(void)
{

    //cb_address_tests();
    //cb_db_tests();
    //cb_request_tests();
    cb_note_ciphertext_tests();
    //cb_confidential_transaction_tests();
    //cb_joinsplit_tests();
    //cb_merkle_tests();
}