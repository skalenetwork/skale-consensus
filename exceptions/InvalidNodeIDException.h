#pragma  once

#include "ProposalProtocolException.h"

class InvalidNodeIDException : public ProposalProtocolException {
public:

    InvalidNodeIDException(const string &_message,  const string& _className) : ProposalProtocolException(_message, _className) {}

};


