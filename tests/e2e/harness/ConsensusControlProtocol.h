#pragma once

#include <initializer_list>
#include <stdexcept>
#include <string>

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

inline const char* toString( ControlCommand command ) {
    switch ( command ) {
    case ControlCommand::Progress:
        return "progress";
    case ControlCommand::Stop:
        return "stop";
    case ControlCommand::InjectAcceptError:
        return "inject-accept-error";
    case ControlCommand::ClearAcceptError:
        return "clear-accept-error";
    case ControlCommand::AcceptErrorCount:
        return "accept-error-count";
    }
    throw std::invalid_argument( "Invalid consensus control command" );
}

inline ControlCommand controlCommandFromString( const std::string& tag ) {
    for ( auto command :
        { ControlCommand::Progress, ControlCommand::Stop, ControlCommand::InjectAcceptError,
            ControlCommand::ClearAcceptError, ControlCommand::AcceptErrorCount } )
        if ( tag == toString( command ) )
            return command;
    throw std::invalid_argument( "Unknown parent command: " + tag );
}

inline const char* toString( ControlEvent event ) {
    switch ( event ) {
    case ControlEvent::Ready:
        return "ready";
    case ControlEvent::Progress:
        return "progress";
    case ControlEvent::Stopped:
        return "stopped";
    case ControlEvent::AcceptErrorEnabled:
        return "accept-error-enabled";
    case ControlEvent::AcceptErrorCleared:
        return "accept-error-cleared";
    case ControlEvent::AcceptErrorCount:
        return "accept-error-count";
    }
    throw std::invalid_argument( "Invalid consensus control event" );
}

inline ControlEvent controlEventFromString( const std::string& tag ) {
    for ( auto event : { ControlEvent::Ready, ControlEvent::Progress, ControlEvent::Stopped,
              ControlEvent::AcceptErrorEnabled, ControlEvent::AcceptErrorCleared,
              ControlEvent::AcceptErrorCount } )
        if ( tag == toString( event ) )
            return event;
    throw std::invalid_argument( "Unknown consensus child event: " + tag );
}

}  // namespace E2ETestUtils
