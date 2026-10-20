#pragma once

#include <array>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace E2ETestUtils {

enum class ControlCommand { Progress, Stop, InjectAcceptError, ClearAcceptError, AcceptErrorCount };
enum class ControlEvent {
    Ready,
    Progress,
    Stopped,
    AcceptErrorEnabled,
    AcceptErrorCleared,
    AcceptErrorCount
};

inline constexpr std::array< std::pair< ControlCommand, std::string_view >, 5 > controlCommandTags{ {
    { ControlCommand::Progress, "progress" },
    { ControlCommand::Stop, "stop" },
    { ControlCommand::InjectAcceptError, "inject-accept-error" },
    { ControlCommand::ClearAcceptError, "clear-accept-error" },
    { ControlCommand::AcceptErrorCount, "accept-error-count" },
} };

inline constexpr std::array< std::pair< ControlEvent, std::string_view >, 6 > controlEventTags{ {
    { ControlEvent::Ready, "ready" },
    { ControlEvent::Progress, "progress" },
    { ControlEvent::Stopped, "stopped" },
    { ControlEvent::AcceptErrorEnabled, "accept-error-enabled" },
    { ControlEvent::AcceptErrorCleared, "accept-error-cleared" },
    { ControlEvent::AcceptErrorCount, "accept-error-count" },
} };

inline const char* toString( ControlCommand command ) {
    for ( const auto& entry : controlCommandTags )
        if ( entry.first == command )
            return entry.second.data();
    throw std::invalid_argument( "Invalid consensus control command" );
}

inline ControlCommand controlCommandFromString( const std::string& tag ) {
    for ( const auto& entry : controlCommandTags )
        if ( entry.second == tag )
            return entry.first;
    throw std::invalid_argument( "Unknown parent command: " + tag );
}

inline const char* toString( ControlEvent event ) {
    for ( const auto& entry : controlEventTags )
        if ( entry.first == event )
            return entry.second.data();
    throw std::invalid_argument( "Invalid consensus control event" );
}

inline ControlEvent controlEventFromString( const std::string& tag ) {
    for ( const auto& entry : controlEventTags )
        if ( entry.second == tag )
            return entry.first;
    throw std::invalid_argument( "Unknown consensus child event: " + tag );
}

}  // namespace E2ETestUtils
