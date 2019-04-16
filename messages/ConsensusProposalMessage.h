#pragma  once
#include <vector>

#include "../SkaleConfig.h"
#include "../Log.h"
#include "../exceptions/FatalError.h"
#include "Message.h"

class Schain;

class ConsensusProposalMessage : public Message {

ptr<vector<bool>>  proposals;

public:
    ConsensusProposalMessage(Schain& subchain, const block_id &blockID, ptr<vector<bool>> proposals);

    const ptr<vector<bool>> &getProposals() const;

};
