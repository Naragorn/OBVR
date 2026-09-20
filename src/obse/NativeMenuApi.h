#pragma once
#include "core/Types.h"

// ABI excerpts retrieved from llde/xOBSE obse/obse/PluginAPI.h and Tasks.h.
// See docs/native-onboarding-prototype.md for sources and binary evidence.
namespace obvr::obse {
// PluginAPI.h: kInterface_Messaging=4, kMessage_PostLoadGame=8.
// Serialization.cpp passes success as (void*)bLoadSucceeded, not bool*.
struct MessagingApi {
 struct Message {const char* sender; UInt32 type, dataLen; void* data;};
 UInt32 version;
 bool (*RegisterListener)(UInt32,const char*,void (*)(Message*));
 bool (*Dispatch)(UInt32,UInt32,void*,UInt32,const char*);
};
struct ConsoleApi {
 UInt32 version;
 bool (*RunScriptLine)(const char*);
 bool (*RunScriptLine2)(const char*, void*, bool);
};
struct TasksApi {
 void* (*EnqueueTask)(void (*)());
 void (*RemoveTask)(void*);
 bool (*IsTaskPresent)(void*);
};
} // namespace obvr::obse
