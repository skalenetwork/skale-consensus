//
// Created by stan on 03.04.18.
//

#include "../../SkaleConfig.h"
#include "../../Log.h"

#include "../../exceptions/FatalError.h"
#include "ChildBVDecidedMessage.h"



using namespace std;

ChildBVDecidedMessage::ChildBVDecidedMessage(bool value, ProtocolInstance &srcProtocolInstance, ptr<ProtocolKey> key) : ChildMessage(BIN_CONSENSUS_COMMIT,
                                                                                                           srcProtocolInstance, key) {
    this->value = value;
}

bool ChildBVDecidedMessage::getValue() const {
    return value;
}
