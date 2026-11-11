// The browser's switches, C:\Data\WebKit\webkit.ini: "name = value" lines,
// with # or ; starting a comment. The file is written with every setting at
// its default when it does not exist; settings are read once, at start.
#pragma once

// Loads aDirectory/webkit.ini, writing it first if it is missing. Marker
// files of earlier builds (an empty C:\Data\WebKit\frame-stats and the
// like) set the matching values in the file written.
void BrowserSettingsLoad(const char* aDirectory);

// on/off settings: frame-stats, curl-verbose, gpu-tiles, composited-video,
// backdrop-filters, timer-deferral, jit-priority.
bool BrowserSetting(const char* aName);

// js-tier: "dfg" (every tier, the default), "baseline" or "llint".
// js-options: JavaScriptCore options as written, "name=value name=value".
const char* BrowserSettingText(const char* aName);

// Calls aSet for each name=value pair of js-options.
void BrowserSettingsForEachJSOption(void (*aSet)(const char* aName, const char* aValue));

// One line naming each setting that is not at its default, and one for each
// line of the file that was not understood. For the log, once it is open.
void BrowserSettingsDescribe(void (*aLog)(const char* aLine));
