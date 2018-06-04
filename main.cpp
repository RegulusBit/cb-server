// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "main.h"

#include "sodium.h"


#include "arith_uint256.h"
#include "consensus/upgrades.h"
#include "consensus/validation.h"
#include "util.h"

#include <sstream>

#include <boost/algorithm/string/replace.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/math/distributions/poisson.hpp>
#include <boost/thread.hpp>
#include <boost/static_assert.hpp>
#include <boost/foreach.hpp>
#include <src/plog/include/plog/Log.h>
#include <src/utilities/utils.h>

using namespace std;

#if defined(NDEBUG)
# error "Zcash cannot be compiled without assertions."
#endif

/**
 * Global state
 */


/**
 * Check a transaction contextually against a set of consensus rules valid at a given block height.
 *
 * Notes:
 * 1. AcceptToMemoryPool calls CheckTransaction and this function.
 * 2. ProcessNewBlock calls AcceptBlock, which calls CheckBlock (which calls CheckTransaction)
 *    and ContextualCheckBlock (which calls this function).
 */
bool ContextualCheckTransaction(const CTransaction& tx, CValidationState &state)
{


    if (!(tx.IsCoinBase() || tx.vjoinsplit.empty())) {
        uint32_t consensusBranchId = 0; // TODO: hardcoded because this feature not needed in this project
        // Empty output script.
        CScript scriptCode;
        uint256 dataToBeSigned;
        try {
            dataToBeSigned = SignatureHash(scriptCode, tx, NOT_AN_INPUT, SIGHASH_ALL, 0, consensusBranchId);
        } catch (std::logic_error ex) {
            return state.DoS(100, error("CheckTransaction(): error computing signature hash"),
                             REJECT_INVALID, "error-computing-signature-hash");
        }

        BOOST_STATIC_ASSERT(crypto_sign_PUBLICKEYBYTES == 32);

        // We rely on libsodium to check that the signature is canonical.
        // https://github.com/jedisct1/libsodium/commit/62911edb7ff2275cccd74bf1c8aefcc4d76924e0
        if (crypto_sign_verify_detached(&tx.joinSplitSig[0],
                                        dataToBeSigned.begin(), 32,
                                        tx.joinSplitPubKey.begin()
        ) != 0) {
            return state.DoS(100, error("CheckTransaction(): invalid joinsplit signature"),
                             REJECT_INVALID, "bad-txns-invalid-joinsplit-signature");
        }
    }
    return true;
}


bool CheckTransaction(const CTransaction& tx, CValidationState &state,
                      libzcash::ProofVerifier& verifier)
{
    ZCJoinSplit *pzcashParams = NULL;

    try {
        pzcashParams = ZCJoinSplit::Prepared("vkfile", "pkfile");
    }catch (...)
    {
        LOG_ERROR << "vkfile or pkfile not found.";
        throw Exception("vkfile or pkfile not found.");

    }
    // Don't count coinbase transactions because mining skews the count
//    if (!tx.IsCoinBase()) {
//        transactionsValidated.increment();
//    }

//    if (!CheckTransactionWithoutProofVerification(tx, state)) {
//        return false;
//    } else {
        // Ensure that zk-SNARKs verify
        BOOST_FOREACH(const JSDescription &joinsplit, tx.vjoinsplit) {
            if (!joinsplit.Verify(*pzcashParams, verifier, tx.joinSplitPubKey)) {
                return state.DoS(100, error("CheckTransaction(): joinsplit does not verify"),
                                 REJECT_INVALID, "bad-txns-joinsplit-verification-failed");
            }
        }
        return true;
//   }
}
