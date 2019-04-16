#ifndef CONSENSUSINTERFACE_H
#define CONSENSUSINTERFACE_H



#include <string>


class ConsensusInterface {
public:
    virtual ~ConsensusInterface() = default;

    virtual void parseFullConfigAndCreateNode( const std::string& fullPathToConfigFile ) = 0;
    virtual void startAll() = 0;
    virtual void bootStrapAll() = 0;
    virtual void exitGracefully() = 0;
};

#endif  // CONSENSUSINTERFACE_H
