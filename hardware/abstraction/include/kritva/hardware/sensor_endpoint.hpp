//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : sensor_endpoint.hpp
// Description : Typed sensor endpoint contract: read one sample.
//
// Component   : KritvaOS Hardware Abstraction
// Module      : Typed Endpoints
// Layer       : Hardware Abstraction
//
// Requirements: DER-501; DER-503; DER-505; DER-507
// API         : kritva::hardware::SensorEndpoint
//
// Author      : KritvaOS
// Created     : 04-10-2026
//==============================================================================

#pragma once

#include <cassert>
#include <type_traits>

#include <kritva/hardware/endpoint.hpp>

namespace kritva::hardware {

/// A sensor endpoint produces one typed `Sample` per read(). `Sample` must be a
/// trivially copyable, default-constructible value type whose units and validity
/// are documented by its own definition; there is deliberately no common
/// "any sample" type.
///
/// read() fails with NOT_READY unless the endpoint is RUNNING (RESOURCE_UNAVAILABLE
/// if it is faulted), without calling the implementation (DER-503). Otherwise
/// do_read()'s result is returned unchanged. Every attempt is counted
/// (statistics) and a failure becomes last_error(). A failing do_read() does not
/// by itself fault the endpoint; an implementation that wants that calls
/// enter_fault().
template <class Sample>
class SensorEndpoint : public Endpoint {
    static_assert(std::is_trivially_copyable_v<Sample> && std::is_default_constructible_v<Sample>,
                  "a sensor sample is a plain value type");

public:
    using sample_type = Sample;

    core::Result<void> read(Sample& out) {
        auto result = check_operational("read");
        if (result) result = do_read(out);
        count_operation(result.has_value());
        if (!result) note_error(result.error());
        return result;
    }

protected:
    SensorEndpoint(EndpointInfo info, core::CapabilitySet capabilities) : Endpoint(std::move(info), std::move(capabilities)) {
        assert(this->info().direction() == EndpointDirection::SENSOR);
    }

    /// Produces one sample while RUNNING.
    virtual core::Result<void> do_read(Sample& out) = 0;
};

} // namespace kritva::hardware
