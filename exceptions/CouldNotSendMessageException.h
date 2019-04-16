#pragma once

#include "ProposalProtocolException.h"

class CouldNotSendMessageException : public ProposalProtocolException{

public:
    CouldNotSendMessageException(const std::string &_message,  const string& _className);

};
