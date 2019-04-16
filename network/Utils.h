#pragma  once

#include <stdint.h>
#include "../abstracttcpserver/ConnectionStatus.h"

class Utils {

    static const uint64_t TIME_START = 2208988800;

public:


    static void checkTime();

    static bool isValidIpAddress(ptr<string>ipAddress);


    static ptr<string> carray2Hex(const uint8_t *d, size_t _len);

    static uint char2int( char _input );
};

