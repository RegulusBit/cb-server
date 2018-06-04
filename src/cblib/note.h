//
// Created by reza on 5/6/18.
//

#ifndef SERVER_NOTE_H
#define SERVER_NOTE_H

#include <zcash/IncrementalMerkleTree.hpp>
#include <primitives/transaction.h>
#include "zcash/NoteEncryption.hpp"
#include "zcash/Note.hpp"


class NoteCiphertext
{

public:
    NoteCiphertext(std::string ciphertext);
    NoteCiphertext(JSDescription joinsplit, int ith);

    std::string serialize(uint256 h_sig, uint256 commitment, uint256 nullifier, ZCIncrementalMerkleTree& merkleTree, unsigned int height);

    ZCNoteDecryption::Ciphertext get_ciphertext(void){return text;};

    uint256 get_root()
    {
        return note_root;
    }

    uint256 get_commitment()
    {
        return note_commitment;
    }

    std::string get_ciphertext_string(void)
    {
        std::string temp;
        std::copy(text.begin(), text.end(), std::back_inserter(temp));
        return temp;
    }

    unsigned int get_height(void)
    {
        return note_height;
    }

    bool get_string_serialization(std::string& cipher)
    {
        if(is_serialized)
        {
            cipher = cipher_text;
            return true;
        } else{
            return false;
        }
    }

    uint256 get_ephemeral_key()
    {
        return ephemeral_key;
    }

    uint256 get_h_sig()
    {
        return h_sig;
    }


private:
    bool is_serialized;
    ZCNoteDecryption::Ciphertext text;
    unsigned int note_height;
    uint256 note_commitment;
    uint256 note_root;
    std::string cipher_text;
    uint256 ephemeral_key;
    uint256 h_sig;

    // belongs to ith note of requested transaction
    // needed for decryption of cipher note by clients
    int ith;

    // TODO: modify the structure of nullifier in class
    // corresponding nullifier of some note: it's equivalent of nullifiers set in zcash
    uint256 correspondent_nullifier;


};


class MerkleTree
{
public:

    MerkleTree():merkletree(){};
    MerkleTree(std::string merkle);
    MerkleTree(ZCIncrementalMerkleTree merkle);

    std::string serialize(void);
    ZCIncrementalMerkleTree get_merkle(void){return merkletree;}
    bool update_merkle(uint256 commitment);

private:
    ZCIncrementalMerkleTree merkletree;

};


std::string UnicodeToUTF8( const std::wstring& ws );
std::wstring UTF8toUnicode(const std::string& s);

// note addition and query methods TODO: get it into proper classes

bool add_note(JSDescription joinsplit, uint256 PubKeyHash, bool is_test);
bool query_note(std::vector<NoteCiphertext> &result_vector, bool is_test);
bool query_note(uint256 root, std::vector<NoteCiphertext> &result_vector, bool is_test);
bool query_note(unsigned int height, std::vector<NoteCiphertext> &result_vector, bool is_test);
bool query_note_one(unsigned int height, std::vector<NoteCiphertext> &result_vector, bool is_test);
bool query_nullifier(uint256 nullifier, std::vector<NoteCiphertext> &result_vector, bool is_test);

#endif //SERVER_NOTE_H
