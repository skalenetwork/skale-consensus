#pragma  once
class Header;
class Connection;

#include "ProposalProtocolException.h"

class OldBlockIDException : public ProposalProtocolException{

   ptr<Header> responseHeader;
public:

    const ptr<Header> &getResponseHeader() const {
        return responseHeader;
    }

    const ptr<Connection> &getConnection() const {
        return connection;
    }

private:
    ptr<Connection> connection;

public:
    OldBlockIDException(const string &_message, const ptr<Header> &responseHeader, const ptr<Connection> &connection,
                        const string& _className)
            : ProposalProtocolException(_message, _className), responseHeader(responseHeader), connection(connection) {}
};
