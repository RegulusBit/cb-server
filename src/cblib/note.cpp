//
// Created by reza on 5/6/18.
//

#include <bsoncxx/builder/basic/document.hpp>
#include <primitives/transaction.h>
#include <codecvt>
#include "note.h"
#include "zcash/NoteEncryption.hpp"
#include "src/utilities/utils.h"
#include "db/db.h"
#include "account.h"


std::wstring UTF8toUnicode(const std::string& s)
{
    std::wstring ws;
    wchar_t wc;
    for( int i = 0;i < s.length(); )
    {
        char c = s[i];
        if ( (c & 0x80) == 0 )
        {
            wc = c;
            ++i;
        }
        else if ( (c & 0xE0) == 0xC0 )
        {
            wc = (s[i] & 0x1F) << 6;
            wc |= (s[i+1] & 0x3F);
            i += 2;
        }
        else if ( (c & 0xF0) == 0xE0 )
        {
            wc = (s[i] & 0xF) << 12;
            wc |= (s[i+1] & 0x3F) << 6;
            wc |= (s[i+2] & 0x3F);
            i += 3;
        }
        else if ( (c & 0xF8) == 0xF0 )
        {
            wc = (s[i] & 0x7) << 18;
            wc |= (s[i+1] & 0x3F) << 12;
            wc |= (s[i+2] & 0x3F) << 6;
            wc |= (s[i+3] & 0x3F);
            i += 4;
        }
        else if ( (c & 0xFC) == 0xF8 )
        {
            wc = (s[i] & 0x3) << 24;
            wc |= (s[i] & 0x3F) << 18;
            wc |= (s[i] & 0x3F) << 12;
            wc |= (s[i] & 0x3F) << 6;
            wc |= (s[i] & 0x3F);
            i += 5;
        }
        else if ( (c & 0xFE) == 0xFC )
        {
            wc = (s[i] & 0x1) << 30;
            wc |= (s[i] & 0x3F) << 24;
            wc |= (s[i] & 0x3F) << 18;
            wc |= (s[i] & 0x3F) << 12;
            wc |= (s[i] & 0x3F) << 6;
            wc |= (s[i] & 0x3F);
            i += 6;
        }
        ws += wc;
    }
    return ws;
}

std::string UnicodeToUTF8( const std::wstring& ws )
{
    std::string s;
    for( int i = 0;i < ws.size(); ++i )
    {
        wchar_t wc = ws[i];
        if ( 0 <= wc && wc <= 0x7f )
        {
            s += (char)wc;
        }
        else if ( 0x80 <= wc && wc <= 0x7ff )
        {
            s += ( 0xc0 | (wc >> 6) );
            s += ( 0x80 | (wc & 0x3f) );
        }
        else if ( 0x800 <= wc && wc <= 0xffff )
        {
            s += ( 0xe0 | (wc >> 12) );
            s += ( 0x80 | ((wc >> 6) & 0x3f) );
            s += ( 0x80 | (wc & 0x3f) );
        }
        else if ( 0x10000 <= wc && wc <= 0x1fffff )
        {
            s += ( 0xf0 | (wc >> 18) );
            s += ( 0x80 | ((wc >> 12) & 0x3f) );
            s += ( 0x80 | ((wc >> 6) & 0x3f) );
            s += ( 0x80 | (wc & 0x3f) );
        }
        else if ( 0x200000 <= wc && wc <= 0x3ffffff )
        {
            s += ( 0xf8 | (wc >> 24) );
            s += ( 0x80 | ((wc >> 18) & 0x3f) );
            s += ( 0x80 | ((wc >> 12) & 0x3f) );
            s += ( 0x80 | ((wc >> 6) & 0x3f) );
            s += ( 0x80 | (wc & 0x3f) );
        }
        else if ( 0x4000000 <= wc && wc <= 0x7fffffff )
        {
            s += ( 0xfc | (wc >> 30) );
            s += ( 0x80 | ((wc >> 24) & 0x3f) );
            s += ( 0x80 | ((wc >> 18) & 0x3f) );
            s += ( 0x80 | ((wc >> 12) & 0x3f) );
            s += ( 0x80 | ((wc >> 6) & 0x3f) );
            s += ( 0x80 | (wc & 0x3f) );
        }
    }
    return s;
}


bool is_valid_utf8(const char * string)
{
    if (!string)
        return true;

    const unsigned char * bytes = (const unsigned char *)string;
    unsigned int cp;
    int num;

    while (*bytes != 0x00)
    {
        if ((*bytes & 0x80) == 0x00)
        {
            // U+0000 to U+007F
            cp = (*bytes & 0x7F);
            num = 1;
        }
        else if ((*bytes & 0xE0) == 0xC0)
        {
            // U+0080 to U+07FF
            cp = (*bytes & 0x1F);
            num = 2;
        }
        else if ((*bytes & 0xF0) == 0xE0)
        {
            // U+0800 to U+FFFF
            cp = (*bytes & 0x0F);
            num = 3;
        }
        else if ((*bytes & 0xF8) == 0xF0)
        {
            // U+10000 to U+10FFFF
            cp = (*bytes & 0x07);
            num = 4;
        }
        else
            return false;

        bytes += 1;
        for (int i = 1; i < num; ++i)
        {
            if ((*bytes & 0xC0) != 0x80)
                return false;
            cp = (cp << 6) | (*bytes & 0x3F);
            bytes += 1;
        }

        if ((cp > 0x10FFFF) ||
            ((cp >= 0xD800) && (cp <= 0xDFFF)) ||
            ((cp <= 0x007F) && (num != 1)) ||
            ((cp >= 0x0080) && (cp <= 0x07FF) && (num != 2)) ||
            ((cp >= 0x0800) && (cp <= 0xFFFF) && (num != 3)) ||
            ((cp >= 0x10000) && (cp <= 0x1FFFFF) && (num != 4)))
            return false;
    }

    return true;
}


NoteCiphertext::NoteCiphertext(std::string ciphertext):is_serialized(true)
{
    // string needed to send to client in response to note requests
    cipher_text = ciphertext;

    Json::Value temp;
    Json::Reader rdr;
    std::wstring dest_cipher;

    rdr.parse(ciphertext, temp);
    LOG_DEBUG << "recovered note cipher text height: " << temp["note_height"];
    ciphertext = temp["encrypted_note"].asString();
    dest_cipher = UTF8toUnicode(ciphertext);
    note_height = temp["note_height"].asUInt();
    ith = temp["ith"].asInt();
    note_commitment = uint256S(temp["commitment"].asString());
    note_root = uint256S(temp["root"].asString());
    ephemeral_key = uint256S(temp["ephemeral_key"].asString());
    h_sig = uint256S(temp["h_sig"].asString());
    correspondent_nullifier = uint256S(temp["nullifier"].asString());

    if(dest_cipher.size() == ZC_CLEN) {
        std::copy(dest_cipher.begin(), dest_cipher.end(), text.begin());
    }else {

        LOG_ERROR << "cipher text size to be created: " << dest_cipher.size();
        Exception("ciphertext is not well formed.");
    }
}

NoteCiphertext::NoteCiphertext(JSDescription joinsplit, int ith):is_serialized(false)
{
    this->ith = ith;
    text = joinsplit.ciphertexts[ith];
    ephemeral_key = joinsplit.ephemeralKey;
    correspondent_nullifier = joinsplit.nullifiers[ith];
}

std::string NoteCiphertext::serialize(uint256 h_sig, uint256 commitment, uint256 nullifier, ZCIncrementalMerkleTree& merkleTree, unsigned int height)
{
    std::wstring temp_string;
    Json::Value temp;
    Json::FastWriter fst;

    std::copy(text.begin(), text.end(), std::back_inserter(temp_string));

    temp["encrypted_note"] = UnicodeToUTF8(temp_string);
    temp["commitment"] = commitment.ToString();

    merkleTree.append(commitment);
    temp["root"] = merkleTree.root().ToString();
    temp["note_height"] = height;
    temp["ephemeral_key"] = ephemeral_key.ToString();
    temp["h_sig"] = h_sig.ToString();

    temp["nullifier"] = nullifier.ToString();
    temp["ith"] = ith;

    cipher_text = fst.write(temp);
    is_serialized = true;

    return fst.write(temp);
}

MerkleTree::MerkleTree(std::string merkle)
{
    Json::Value temp;
    Json::Reader rdr;
    std::wstring dest_merkle;
    rdr.parse(merkle, temp);

    using convert_type = std::codecvt_utf8<wchar_t>;
    std::wstring_convert<convert_type, wchar_t> converter;


    LOG_INFO << "is recovered merkle valid utf-8: " << is_valid_utf8(temp["merkletree"].asCString());
    dest_merkle = UTF8toUnicode(temp["merkletree"].asString());

    std::istringstream merkle_stream(converter.to_bytes(dest_merkle));

    merkletree.Unserialize(merkle_stream, 1, 1);
}

MerkleTree::MerkleTree(ZCIncrementalMerkleTree merkle):merkletree()
{
    merkletree = merkle;
}

std::string MerkleTree::serialize()
{
    std::wstring temp_string;
    Json::Value temp_tree;
    std::ostringstream temp;
    Json::FastWriter fst;

    merkletree.Serialize(temp, 1, 1);

    std::copy(temp.str().begin(), temp.str().end(), std::back_inserter(temp_string));

    LOG_INFO << "is merkle valid utf-8: " << is_valid_utf8(UnicodeToUTF8(temp_string).data());

    temp_tree["merkletree"] = UnicodeToUTF8(temp_string);

    return fst.write(temp_tree);

}

bool MerkleTree::update_merkle(uint256 commitment)
{
    try {
        merkletree.append(commitment);
    }catch (...){
        Exception("appending new commitment to merkletree falied.");
        return false;
    }

    return true;
}

bool query_note(unsigned int height, std::vector<NoteCiphertext> &result_vector, bool is_test)
{
    collection result_type;
    if(is_test)
        result_type = collection::notes_test;
    else
        result_type = collection::notes;

    LOG_DEBUG << "query note from database.";
    bsoncxx::builder::basic::document condition;
    mongocxx::options::find opt;
    bool result;

    // define condition for query based on note height
    condition.append(bsoncxx::builder::basic::kvp("note_height",[&height](bsoncxx::builder::basic::sub_document temp){
        temp.append(bsoncxx::builder::basic::kvp("$gt", static_cast<int>(height)));
    }));


    result = database::get_instance().query<NoteCiphertext>(result_vector, result_type, condition.view(),opt , "NOVALUE", false);

    if(!result_vector.size())
        LOG_ERROR << "query notes did not recovered.";

    return result;
}

bool query_note(uint256 root, std::vector<NoteCiphertext> &result_vector, bool is_test)
{
    collection result_type;
    if(is_test)
        result_type = collection::notes_test;
    else
        result_type = collection::notes;

    LOG_DEBUG << "query note from database.";
    bsoncxx::builder::basic::document condition;
    mongocxx::options::find opt;
    bool result;

    // define condition for query based on note root
    condition.append(bsoncxx::builder::basic::kvp("root", root.ToString()));


    result = database::get_instance().query<NoteCiphertext>(result_vector, result_type, condition.view(),opt , "NOVALUE", false);

    if(!result_vector.size())
        LOG_ERROR << "query notes did not recovered.";

    return result;
}

bool query_nullifier(uint256 nullifier, std::vector<NoteCiphertext> &result_vector, bool is_test)
{
    collection result_type;
    if(is_test)
        result_type = collection::notes_test;
    else
        result_type = collection::notes;

    LOG_DEBUG << "query note from database.";
    bsoncxx::builder::basic::document condition;
    mongocxx::options::find opt;
    bool result;

    // define condition for query based on note root
    condition.append(bsoncxx::builder::basic::kvp("nullifier", nullifier.ToString()));


    result = database::get_instance().query<NoteCiphertext>(result_vector, result_type, condition.view(),opt , "NOVALUE", false);

    if(!result_vector.size())
        LOG_ERROR << "query notes did not recovered.";

    return result;
}

bool query_note_one(unsigned int height, std::vector<NoteCiphertext> &result_vector, bool is_test)
{
    collection result_type;
    if(is_test)
        result_type = collection::notes_test;
    else
        result_type = collection::notes;

    LOG_DEBUG << "query note from database.";
    bsoncxx::builder::basic::document condition;
    mongocxx::options::find opt;
    bool result;

    // define condition for query based on note root
    condition.append(bsoncxx::builder::basic::kvp("note_height", int(height)));


    result = database::get_instance().query<NoteCiphertext>(result_vector, result_type, condition.view(),opt , "NOVALUE", false);

    if(!result_vector.size())
        LOG_ERROR << "query notes did not recovered.";

    return result;
}

//get the latest note
bool query_note(std::vector<NoteCiphertext> &result_vector, bool is_test)
{
    collection result_type;
    if(is_test)
        result_type = collection::notes_test;
    else
        result_type = collection::notes;

    LOG_DEBUG << "query note from database.";
    bsoncxx::builder::basic::document condition;
    bsoncxx::document::view_or_value document{};
    mongocxx::options::find opt;
    bool result;

    // define condition for query based on note root
    condition.append(bsoncxx::builder::basic::kvp("note_height", -1));
    opt.sort(condition.view());
    opt.limit(1);


    result = database::get_instance().query<NoteCiphertext>(result_vector, result_type, document, opt, "NOVALUE", false);

    if(!result_vector.size())
        LOG_ERROR << "query notes did not recovered.";

    return result;
}


bool add_note(JSDescription joinsplit, uint256 PubKeyHash, bool is_test)
{
    collection result_type;
    if(is_test)
        result_type = collection::notes_test;
    else
        result_type = collection::notes;

    LOG_DEBUG << "adding note to database " << collector[result_type] << ".";
    bool final_result = true;
    std::vector<NoteCiphertext> query_result;
    ServerEngine engine(is_test);
    unsigned int last_added_height;
    ZCIncrementalMerkleTree merkletree = engine.get_merkle();

    if(joinsplit.commitments.size() !=2 || joinsplit.ciphertexts.size() != 2)
    {
        LOG_ERROR << "add_note: joinsplit description information is not complete.";
        return false;
    }

    if(!query_note(query_result, is_test))
    {
        last_added_height = 0;
    } else
    {
        // there is zero or one result by default
        last_added_height = query_result[0].get_height();
    }

    for(int i = 0 ; i < 2 ; i++)
    {
        NoteCiphertext ciphernote(joinsplit, i);
        std::string serialized_cipher = ciphernote.serialize(joinsplit.h_sig(*engine.get_zkparams(), PubKeyHash),
                                                             joinsplit.commitments[i], joinsplit.nullifiers[i], merkletree , last_added_height+i+1);
        LOG_WARNING << "updated merkle root: " << merkletree.root().ToString();

        if(!database::get_instance().add_request(serialized_cipher, collector[result_type], "NOVALUE"))
        {
            final_result = false;
            break;
        }

    }

    return final_result;
}


