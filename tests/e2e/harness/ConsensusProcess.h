#pragma once

#include "ControlChannel.h"
#include "HarnessTimeouts.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <sys/types.h>
#include <vector>

namespace E2ETestUtils {

class ConsensusProcess {
    pid_t pid = -1;
    ControlChannel control;
    std::filesystem::path directory;
    std::string bindIP;
    bool stoppedSuccessfully = false;
    std::optional< int > childStatus;
    int waitError = 0;
    bool killRequested = false;

    void prepareEnvironment( const std::string& sourceRoot );
    void launchChild(
        const std::string& executable, const std::vector< std::string >& extraArguments );
    void waitUntilReady( Deadline deadline );
    uint64_t receiveHeight( ControlEvent event, Deadline deadline );
    void collectChildStatus( bool blocking ) noexcept;
    void terminateChild() noexcept;
    std::string describeChildStatus() const;
    [[noreturn]] void rethrowChildFailure( const char* operation );
    void cleanup( bool preserveDiagnostics ) noexcept;

public:
    static constexpr uint16_t BASE_PORT = 12310;

    // Extra arguments follow the configuration and data directory arguments.
    ConsensusProcess( const std::string& executable, const std::string& sourceRoot,
        const std::vector< std::string >& extraArguments = {} );
    ~ConsensusProcess();
    ConsensusProcess( const ConsensusProcess& ) = delete;
    ConsensusProcess& operator=( const ConsensusProcess& ) = delete;

    const std::string& address() const { return bindIP; }
    std::filesystem::path logsDirectory() const { return directory / "logs"; }
    void injectAcceptError( Deadline deadline );
    void clearAcceptError( Deadline deadline );
    uint64_t acceptErrorCount( Deadline deadline );
    uint64_t progress(
        Deadline deadline = deadlineIn( HarnessTimeouts::PROGRESS_RESPONSE ) );
    void stop(
        Deadline deadline = deadlineIn( HarnessTimeouts::SHUTDOWN ) );
};

} // namespace E2ETestUtils
