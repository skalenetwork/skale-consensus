#pragma once

#include "ProposalProtocolException.h"

class InvalidSchainException : public ProposalProtocolException{
public:
    InvalidSchainException(const string &_message,  const string& _className) : ProposalProtocolException(_message, _className) {}

};



