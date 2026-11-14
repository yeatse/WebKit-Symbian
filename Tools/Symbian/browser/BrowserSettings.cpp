#include "BrowserSettings.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

namespace {

struct Setting {
    const char* name;
    const char* defaultValue;
    // Space-separated values accepted; null for on/off; "*" for any text,
    // kept as written.
    const char* choices;
    // The marker file of earlier builds and the value it stood for.
    const char* legacyFile;
    const char* legacyValue;
    const char* comment;
    char value[160];
};

Setting settings[] = {
    { "frame-stats", "off", nullptr, "frame-stats", "on",
        "Log frame timings, the main-thread profile and decode times to log.txt.", "" },
    { "profile", "off", nullptr, nullptr, nullptr,
        "Sample where the UI thread runs and write the profile to log.txt at exit.", "" },
    { "curl-verbose", "off", nullptr, "curl-verbose", "on",
        "Write curl's own trace to curl.txt.", "" },
    { "gpu-tiles", "on", nullptr, "no-gpu-tiles", "off",
        "Composite every page with EGL and scroll cached GPU tiles.", "" },
    { "composited-video", "on", nullptr, "no-composited-video", "off",
        "Show video in a window under the page, which keeps EGL while it plays.", "" },
    { "backdrop-filters", "off", nullptr, "backdrop-filters", "on",
        "Draw CSS backdrop-filter (about 10 ms a frame on a Nokia 808).", "" },
    { "timer-deferral", "on", nullptr, "no-timer-deferral", "off",
        "Hold page timers while the user touches or scrolls.", "" },
    { "jit-priority", "on", nullptr, nullptr, nullptr,
        "Run JIT compiler threads at the UI thread's priority instead of below it.", "" },
    { "js-tier", "dfg", "dfg baseline llint", nullptr, nullptr,
        "JavaScript tiers: dfg (all), baseline (no DFG compiler) or llint (interpreter only).", "" },
    { "js-options", "", "*", nullptr, nullptr,
        "JavaScriptCore options for experiments, as name=value separated by spaces.", "" },
};

bool isText(const Setting& setting)
{
    return setting.choices && !strcmp(setting.choices, "*");
}

constexpr unsigned maximumProblems = 4;
char problems[maximumProblems][64];
unsigned problemCount;

void note(const char* format, const char* text)
{
    if (problemCount < maximumProblems)
        snprintf(problems[problemCount++], sizeof(problems[0]), format, text);
}

Setting* find(const char* name)
{
    for (auto& setting : settings) {
        if (!strcmp(setting.name, name))
            return &setting;
    }
    return nullptr;
}

bool hasWord(const char* words, const char* word)
{
    size_t length = strlen(word);
    for (const char* at = words; (at = strstr(at, word)); at += length) {
        if ((at == words || at[-1] == ' ') && (!at[length] || at[length] == ' '))
            return true;
    }
    return false;
}

// The setting's own spelling of a value, or null if it is not one.
const char* canonical(const Setting& setting, const char* value)
{
    if (isText(setting))
        return strlen(value) < sizeof(setting.value) ? value : nullptr;
    if (setting.choices)
        return *value && hasWord(setting.choices, value) ? value : nullptr;
    if (hasWord("on 1 true yes", value))
        return "on";
    if (hasWord("off 0 false no", value))
        return "off";
    return nullptr;
}

char* trim(char* text)
{
    while (isspace(static_cast<unsigned char>(*text)))
        ++text;
    char* end = text + strlen(text);
    while (end > text && isspace(static_cast<unsigned char>(end[-1])))
        --end;
    *end = 0;
    return text;
}

void set(Setting& setting, const char* value)
{
    snprintf(setting.value, sizeof(setting.value), "%s", value);
}

void parseLine(char* line)
{
    if (char* comment = strpbrk(line, "#;"))
        *comment = 0;
    char* name = trim(line);
    // Blank, or a [section] heading, which groups nothing here.
    if (!*name || *name == '[')
        return;
    char* equals = strchr(name, '=');
    if (!equals) {
        note("settings: no value in '%.32s'", name);
        return;
    }
    *equals = 0;
    name = trim(name);
    char* value = trim(equals + 1);
    for (char* at = name; *at; ++at)
        *at = static_cast<char>(tolower(static_cast<unsigned char>(*at)));
    Setting* setting = find(name);
    if (!setting) {
        note("settings: unknown name '%.32s'", name);
        return;
    }
    if (!isText(*setting)) {
        for (char* at = value; *at; ++at)
            *at = static_cast<char>(tolower(static_cast<unsigned char>(*at)));
    }
    const char* accepted = canonical(*setting, value);
    if (!accepted) {
        note("settings: bad value for '%.32s'", name);
        return;
    }
    set(*setting, accepted);
}

bool exists(const char* directory, const char* name)
{
    char path[256];
    snprintf(path, sizeof(path), "%s/%s", directory, name);
    FILE* file = fopen(path, "rb");
    if (!file)
        return false;
    fclose(file);
    return true;
}

void writeDefaults(const char* directory, const char* path)
{
    for (auto& setting : settings) {
        if (setting.legacyFile && exists(directory, setting.legacyFile))
            set(setting, setting.legacyValue);
    }
    FILE* file = fopen(path, "wb");
    if (!file)
        return;
    fputs("# WebKit browser settings. To change one, remove the # before its name,\r\n"
        "# edit the value and restart the browser. Delete this file to get every\r\n"
        "# setting back at its default.\r\n", file);
    for (auto& setting : settings) {
        // A setting left at its default stays commented out, so that it
        // follows the default if a later build changes it.
        if (isText(setting)) {
            fprintf(file, "\r\n# %s\r\n# Default: none.\r\n#%s = \r\n", setting.comment, setting.name);
            continue;
        }
        fprintf(file, "\r\n# %s\r\n# %s; default %s.\r\n%s%s = %s\r\n", setting.comment,
            setting.choices ? setting.choices : "on or off", setting.defaultValue,
            strcmp(setting.value, setting.defaultValue) ? "" : "#", setting.name, setting.value);
    }
    fclose(file);
}

} // namespace

void BrowserSettingsLoad(const char* aDirectory)
{
    problemCount = 0;
    for (auto& setting : settings)
        set(setting, setting.defaultValue);
    char path[256];
    snprintf(path, sizeof(path), "%s/webkit.ini", aDirectory);
    FILE* file = fopen(path, "rb");
    if (!file) {
        writeDefaults(aDirectory, path);
        return;
    }
    // Editors on the phone save UTF-16 with a byte order mark; a PC may add
    // a UTF-8 one. Names and values are ASCII: keep those bytes only.
    static char text[4096];
    size_t length = fread(text, 1, sizeof(text) - 1, file);
    fclose(file);
    const auto* bytes = reinterpret_cast<const unsigned char*>(text);
    size_t count = 0;
    auto keep = [&](unsigned character) {
        // Other characters, in a comment say, must not look like syntax.
        text[count++] = character && character < 0x80 ? static_cast<char>(character) : '?';
    };
    bool littleEndian = length >= 2 && bytes[0] == 0xff && bytes[1] == 0xfe;
    bool bigEndian = length >= 2 && bytes[0] == 0xfe && bytes[1] == 0xff;
    if (littleEndian || bigEndian) {
        for (size_t i = 2; i + 1 < length; i += 2)
            keep(littleEndian ? bytes[i] | (bytes[i + 1] << 8) : (bytes[i] << 8) | bytes[i + 1]);
    } else {
        bool mark = length >= 3 && bytes[0] == 0xef && bytes[1] == 0xbb && bytes[2] == 0xbf;
        for (size_t i = mark ? 3 : 0; i < length; ++i)
            keep(bytes[i]);
    }
    text[count] = 0;
    for (char* line = text; line && *line;) {
        char* end = strpbrk(line, "\r\n");
        if (end)
            *end++ = 0;
        parseLine(line);
        line = end;
    }
}

bool BrowserSetting(const char* aName)
{
    const Setting* setting = find(aName);
    return setting && !strcmp(setting->value, "on");
}

const char* BrowserSettingText(const char* aName)
{
    const Setting* setting = find(aName);
    return setting ? setting->value : "";
}

void BrowserSettingsForEachJSOption(void (*aSet)(const char* aName, const char* aValue))
{
    char options[sizeof(settings[0].value)];
    snprintf(options, sizeof(options), "%s", BrowserSettingText("js-options"));
    for (char* option = strtok(options, " ,\t"); option; option = strtok(nullptr, " ,\t")) {
        char* equals = strchr(option, '=');
        if (!equals || equals == option) {
            note("settings: js-options needs name=value, not '%.32s'", option);
            continue;
        }
        *equals = 0;
        aSet(option, equals + 1);
    }
}

void BrowserSettingsDescribe(void (*aLog)(const char* aLine))
{
    char line[256] = "settings:";
    size_t used = strlen(line);
    bool any = false;
    for (const auto& setting : settings) {
        any |= isText(setting) && *setting.value;
        if (!strcmp(setting.value, setting.defaultValue) || isText(setting))
            continue;
        int written = snprintf(line + used, sizeof(line) - used, " %s=%s", setting.name, setting.value);
        if (written > 0 && static_cast<size_t>(written) < sizeof(line) - used)
            used += written;
        any = true;
    }
    if (!any)
        snprintf(line + used, sizeof(line) - used, " all at their defaults");
    aLog(line);
    // Text can be long: a line of its own.
    for (const auto& setting : settings) {
        if (!isText(setting) || !*setting.value)
            continue;
        snprintf(line, sizeof(line), "settings: %s = %s", setting.name, setting.value);
        aLog(line);
    }
    for (unsigned i = 0; i < problemCount; ++i)
        aLog(problems[i]);
}
