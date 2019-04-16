#pragma once

#include "ProposalProtocolException.h"

class InvalidMessageFormatException : public ProposalProtocolException {
public:
    InvalidMessageFormatException(const string &_message,  const string& _className) :
                ProposalProtocolException(_message ,  _className) {}

};

