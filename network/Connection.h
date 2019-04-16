#pragma  once

class Connection {

    static uint64_t totalConnections;



    file_descriptor descriptor;
    ptr<string> ip;

public:

    Connection(unsigned int descriptor, ptr<string>ip);

    virtual ~Connection();

    file_descriptor getDescriptor();

    ptr<string> getIP();

    static void incrementTotalConnections();

    static void decrementTotalConnections();

    static uint64_t getTotalConnections();

};

