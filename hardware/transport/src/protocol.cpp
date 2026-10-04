//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : protocol.cpp
// Description : Protocol contract names.
//
// Component   : KritvaOS Hardware Abstraction
// Module      : Transport Protocol
// Layer       : Hardware Abstraction
//
// Requirements: PR-002
// API         : kritva::hardware::transport
//
// Author      : KritvaOS
// Created     : 04-10-2026
//==============================================================================

#include <kritva/hardware/transport/protocol.hpp>

namespace kritva::hardware::transport {

const char* message_type_name(MessageType type) noexcept {
    switch (type) {
        case MessageType::HELLO: return "HELLO";                           case MessageType::HELLO_ACK: return "HELLO_ACK";
        case MessageType::DISCOVERY_REQUEST: return "DISCOVERY_REQUEST";   case MessageType::DISCOVERY_RESPONSE: return "DISCOVERY_RESPONSE";
        case MessageType::CONFIGURE_REQUEST: return "CONFIGURE_REQUEST";   case MessageType::CONFIGURE_RESPONSE: return "CONFIGURE_RESPONSE";
        case MessageType::INITIALIZE_REQUEST: return "INITIALIZE_REQUEST"; case MessageType::INITIALIZE_RESPONSE: return "INITIALIZE_RESPONSE";
        case MessageType::START_REQUEST: return "START_REQUEST";           case MessageType::START_RESPONSE: return "START_RESPONSE";
        case MessageType::STOP_REQUEST: return "STOP_REQUEST";             case MessageType::STOP_RESPONSE: return "STOP_RESPONSE";
        case MessageType::SHUTDOWN_REQUEST: return "SHUTDOWN_REQUEST";     case MessageType::SHUTDOWN_RESPONSE: return "SHUTDOWN_RESPONSE";
        case MessageType::READ_REQUEST: return "READ_REQUEST";             case MessageType::READ_RESPONSE: return "READ_RESPONSE";
        case MessageType::WRITE_REQUEST: return "WRITE_REQUEST";           case MessageType::WRITE_RESPONSE: return "WRITE_RESPONSE";
        case MessageType::OBSERVE_REQUEST: return "OBSERVE_REQUEST";       case MessageType::OBSERVE_RESPONSE: return "OBSERVE_RESPONSE";
        case MessageType::FAULT_EVENT: return "FAULT_EVENT";               case MessageType::HEARTBEAT: return "HEARTBEAT";
        case MessageType::PROTOCOL_ERROR: return "PROTOCOL_ERROR";
    }
    return "INVALID";
}

const char* session_state_name(SessionState state) noexcept {
    switch (state) {
        case SessionState::DISCONNECTED: return "DISCONNECTED"; case SessionState::CONNECTING: return "CONNECTING";
        case SessionState::NEGOTIATING: return "NEGOTIATING";   case SessionState::CONNECTED: return "CONNECTED";
        case SessionState::DEGRADED: return "DEGRADED";
    }
    return "INVALID";
}

} // namespace kritva::hardware::transport
