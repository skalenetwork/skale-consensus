//
// Created by stan on 23.11.18.
//

#include "../SkaleConfig.h"
#include "../Log.h"
#include "../exceptions/FatalError.h"
#include "CouldNotSendMessageException.h"

CouldNotSendMessageException::CouldNotSendMessageException(const std::string &_message,  const string& _className) :
ProposalProtocolException(_message, _className) {}
