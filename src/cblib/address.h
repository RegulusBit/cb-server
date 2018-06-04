#ifndef ADDRESS_H
#define ADDRESS_H


#include "zcash/Address.hpp"
#include "jsoncpp/json/value.h"
#include "jsoncpp/json/json.h"
#include "plog/Log.h"
#include <plog/Appenders/ColorConsoleAppender.h>


class CBPaymentAddress
{
public:
    CBPaymentAddress();
    CBPaymentAddress(std::string);

    libzcash::PaymentAddress get_pk(void)
    {
        return pk;
    }

    libzcash::SpendingKey get_sk(void)
    {
        return sk;
    }

    Json::Value serialize();

private:
    libzcash::SpendingKey sk;
    libzcash::PaymentAddress pk;
};


#endif