#include "address.h"

CBPaymentAddress::CBPaymentAddress()
{
    sk = libzcash::SpendingKey::random();
    pk = sk.address();

}

CBPaymentAddress::CBPaymentAddress(std::string str)
{
    pk.a_pk = uint256S(str);
}

Json::Value CBPaymentAddress::serialize()
{
    Json::Value serialized;

    serialized["pk"] = pk.a_pk.ToString();

    return serialized;
}