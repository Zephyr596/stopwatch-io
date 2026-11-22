#pragma once
#if __has_include("LocalConfig.h")
#include "LocalConfig.h"
#else
#include "LocalConfig.example.h"
#endif

#ifndef SW_DEFAULT_API
#define SW_DEFAULT_API false
#endif

#ifndef SW_CODEX_LAN_KEY
#define SW_CODEX_LAN_KEY ""
#endif
