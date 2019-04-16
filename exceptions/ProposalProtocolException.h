#pragma  once
#include "NetworkProtocolException.h"

class ProposalProtocolException : public NetworkProtocolException {
public:
    ProposalProtocolException(const std::string &_message,  const string& _className);

};

