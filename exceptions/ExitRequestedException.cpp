
#include "../SkaleConfig.h"
#include "../Log.h"
#include "../exceptions/FatalError.h"
#include "ExitRequestedException.h"



ExitRequestedException::ExitRequestedException() : Exception("Exit requested",  "" ){

}
