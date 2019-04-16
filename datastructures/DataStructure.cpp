#include "../SkaleConfig.h"

#include "../chains/Schain.h"

#include "DataStructure.h"


DataStructure::DataStructure() {
    objectCreationTime = Schain::getHighResolutionTime();
}

uint64_t DataStructure::getObjectCreationTime() const {
    return objectCreationTime;
}
