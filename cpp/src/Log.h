#pragma once

#include "crysystem/SSystemGlobalEnvironment.h"

/// Writes a line to the game log, prefixed with the mod name.
///
/// The global environment is resolved per call: it does not exist yet while
/// KCSE loads plugins, and a cached pointer taken at that point would be null
/// for the rest of the session.
#define SR_LOG(fmt, ...)                                                              \
    do {                                                                              \
        if (auto* _env = SSystemGlobalEnvironment::GetInstance(); _env && _env->pLog) \
            _env->pLog->LogAlways("[SavegameRenamer] " fmt, ##__VA_ARGS__);           \
    } while (0)
