//
// Created by skale on 24.03.22.
//

#ifndef SKALED_INVALIDSIGNATUREEXCEPTION_H
#define SKALED_INVALIDSIGNATUREEXCEPTION_H

#include "InvalidStateException.h"

class InvalidSignatureException : public InvalidStateException {
public:
    InvalidSignatureException( const std::string& _message, const std::string& _className )
        : InvalidStateException( _message, _className ){};
};


#endif  // SKALED_INVALIDSIGNATUREEXCEPTION_H
