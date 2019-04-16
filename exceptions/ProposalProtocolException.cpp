//
// Created by stan on 23.11.18.
//

#include "../SkaleConfig.h"
#include "../Log.h"
#include "../exceptions/FatalError.h"
#include "ProposalProtocolException.h"

ProposalProtocolException::ProposalProtocolException(const std::string &_message, const string &_className) :
        NetworkProtocolException(
                _message, _className) {}
