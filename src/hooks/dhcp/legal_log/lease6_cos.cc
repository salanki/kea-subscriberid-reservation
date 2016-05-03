// Copyright (C) 2016 Internet Systems Consortium, Inc. ("ISC")
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

/// @file lease6_co.cc Defines lease6_select and lease6_renew callout functions.

#include <config.h>
#include <hooks/hooks.h>
#include <dhcp/pkt6.h>
#include <legal_log_log.h>
#include <legal_file.h>

#include <sstream>

using namespace isc;
using namespace dhcp;
using namespace hooks;
using namespace legal_log;
using namespace std;

/// @brief Pointer to the registry instance.
extern LegalFilePtr legal_file;

// Functions accessed by the hooks framework use C linkage to avoid the name
// mangling that accompanies use of the C++ compiler as well as to avoid
// issues related to namespaces.
extern "C" {

/// @brief  This callout is called at the "lease6_select" hook.
///
/// @param handle CalloutHandle which provides access to context.
///
/// @return 0 upon success, non-zero otherwise.
int lease6_select(CalloutHandle& handle) {
    if (!legal_file) {
        LOG_ERROR(legal_log_logger, LEGAL_FILE_HOOK_LEASE6_SELECT_NO_LEGAL_FILE);
        return (1);
    }

    try {
        std::stringstream stream;
        stream << "lease6_select: address granted";
        legal_file->writeln(stream.str());
    } catch (const std::exception& ex) {
        LOG_ERROR(legal_log_logger, LEGAL_FILE_HOOK_LEASE6_SELECT_WRITE_ERROR)
                  .arg(ex.what());
        return (1);
    }

    return (0);
}

/// @brief  This callout is called at the "lease6_renew" hook.
///
/// @param handle CalloutHandle which provides access to context.
///
/// @return 0 upon success, non-zero otherwise.
int lease6_renew(CalloutHandle& handle) {
    if (!legal_file) {
        LOG_ERROR(legal_log_logger, LEGAL_FILE_HOOK_LEASE6_RENEW_NO_LEGAL_FILE);
        return (1);
    }

    try {
        std::stringstream stream;
        stream << "lease6_renew: address renewed";
        legal_file->writeln(stream.str());
    } catch (const std::exception& ex) {
        LOG_ERROR(legal_log_logger, LEGAL_FILE_HOOK_LEASE6_RENEW_WRITE_ERROR)
                  .arg(ex.what());
        return (1);
    }

    return (0);
}



} // end extern "C"