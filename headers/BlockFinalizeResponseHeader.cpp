//
// Created by stan on 18.03.18.
//


#include "../SkaleConfig.h"
#include "../Log.h"
#include "../thirdparty/json.hpp"
#include "../exceptions/InvalidArgumentException.h"

#include "AbstractBlockRequestHeader.h"
#include "BlockFinalizeResponseHeader.h"

void BlockFinalizeResponseHeader::setSigShare(const ptr<string> &_sigShare) {

    if (!_sigShare || _sigShare->size() < 10) {
        BOOST_THROW_EXCEPTION(InvalidArgumentException("Null or misformatted sig share", __CLASS_NAME__));
    }

    sigShare = _sigShare;
}

void BlockFinalizeResponseHeader::addFields(nlohmann::json &jsonRequest) {

    Header::addFields(jsonRequest);

    if (status != CONNECTION_SUCCESS)
        return;

    if (!sigShare) {
        BOOST_THROW_EXCEPTION(InvalidArgumentException("Null sig share", __CLASS_NAME__));
    }


    if (!sigShare || sigShare->size() < 10) {
        BOOST_THROW_EXCEPTION(InvalidArgumentException("Misformatted sig share", __CLASS_NAME__));
    }

    jsonRequest["sigShare"] = *sigShare;

}
