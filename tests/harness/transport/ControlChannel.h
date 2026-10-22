#pragma once

#include "ConsensusControlProtocol.h"
#include "SocketIO.h"
#include "network/Socket.h"
#include "thirdparty/json.hpp"

#include <cstdint>
#include <string>

namespace E2ETestUtils {

// Owns a framed parent/child control socket. Public message operations close it on failure.
// Both formats share this socket: the parent sends plain-text command tags to request actions;
// the child sends JSON responses with event tags and optional data, including unsolicited Ready.
// Callers serialize exchanges and share one absolute deadline across each request/response.
class ControlChannel {
    Socket socket;

    // Sends a validated length-prefixed body; the header and body share the deadline.
    void sendFrame( const std::string& body, Deadline deadline );
    // Reads a length-prefixed body, rejecting empty frames and bodies exceeding MAX_FRAME_SIZE.
    std::string receiveFrame( Deadline deadline );
    // Throws if no socket is owned.
    void requireOpen() const;

public:
    static constexpr std::uint32_t MAX_FRAME_SIZE = 4096;

    // Creates a closed channel for later move assignment.
    ControlChannel() = default;
    // Takes ownership of an already-connected socket endpoint.
    explicit ControlChannel( Socket endpoint ) noexcept;

    // Closes the endpoint, reporting close errors to stderr; safe to repeat.
    void close() noexcept;

    // ------ Commands: plain-text tags ------
    // Sends a typed command as a length-prefixed text tag.
    void sendCommand( ControlCommand command, Deadline deadline );
    // Reads a command tag and rejects unknown values.
    ControlCommand receiveCommand( Deadline deadline );

    // ------ Responses: JSON objects with event tags and optional payload ------
    // Adds the event tag to the JSON payload and sends it as one frame.
    void sendResponse( ControlEvent event, nlohmann::json response, Deadline deadline );
    // Parses a JSON response and verifies its event; payload fields are validated by the caller.
    nlohmann::json receiveResponse( ControlEvent expected, Deadline deadline );
};

} // namespace E2ETestUtils
