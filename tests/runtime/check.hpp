//==============================================================================
// Copyright (c) 2026 KritvaOS
// SPDX-License-Identifier: Apache-2.0
//
// File        : check.hpp
// Description : Minimal test check macro that stays active in every build type (test only).
//
// Component   : KritvaOS Runtime
// Module      : Tests
// Layer       : Application Runtime
//
// Requirements: RR-TST-001; RR-TST-008
// API         : KRITVA_CHECK
//
// Author      : KritvaOS
// Created     : 04-10-2026
//==============================================================================

#pragma once

#include <cstdio>
#include <cstdlib>

// Unlike assert(), never compiled out by NDEBUG, so tests cannot pass vacuously.
#define KRITVA_CHECK(cond)                                                      \
    do {                                                                        \
        if (!(cond)) {                                                          \
            std::fprintf(stderr, "%s:%d: CHECK FAILED: %s\n", __FILE__, __LINE__, #cond); \
            std::exit(1);                                                       \
        }                                                                       \
    } while (false)
