#include <Union/Hook.h>
#include <ZenGin/zGothicAPI.h>

#ifndef __G2A
#error "ZenGin DevBridge is built for Gothic II: Night of the Raven only."
#endif

#define GOTHIC_NAMESPACE Gothic_II_Addon
HOOKSPACE(Gothic_II_Addon, GetGameVersion() == Engine_G2A);
#include "Plugin.ipp"
#undef GOTHIC_NAMESPACE

HOOKSPACE(Global, true);
