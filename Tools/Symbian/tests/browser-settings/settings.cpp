// Host test for the browser's webkit.ini reader.
#include "BrowserSettings.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

static std::vector<std::string> lines;
static void collect(const char* line) { lines.push_back(line); }

static void write(const std::string& path, const char* text)
{
    FILE* file = fopen(path.c_str(), "wb");
    assert(file);
    fputs(text, file);
    fclose(file);
}

static std::string read(const std::string& path)
{
    std::string text;
    FILE* file = fopen(path.c_str(), "rb");
    assert(file);
    char buffer[512];
    size_t count;
    while ((count = fread(buffer, 1, sizeof(buffer), file)))
        text.append(buffer, count);
    fclose(file);
    return text;
}

int main(int argc, char** argv)
{
    assert(argc == 2);
    std::string directory = argv[1];
    std::string ini = directory + "/webkit.ini";

    // No file: defaults, with earlier builds' marker files carried over.
    write(directory + "/frame-stats", "");
    write(directory + "/no-gpu-tiles", "");
    BrowserSettingsLoad(directory.c_str());
    assert(BrowserSetting("frame-stats") && !BrowserSetting("gpu-tiles"));
    assert(BrowserSetting("composited-video") && BrowserSetting("timer-deferral"));
    assert(!BrowserSetting("backdrop-filters") && !BrowserSetting("jit-priority") && !BrowserSetting("curl-verbose"));
    assert(std::string(BrowserSettingText("js-tier")) == "dfg");
    std::string written = read(ini);
    assert(written.find("frame-stats = on\r\n") != std::string::npos);
    assert(written.find("gpu-tiles = off\r\n") != std::string::npos);
    assert(written.find("js-tier = dfg\r\n") != std::string::npos);
    BrowserSettingsDescribe(collect);
    assert(lines.size() == 1 && lines[0] == "settings: frame-stats=on gpu-tiles=off");

    // The file written reads back the same; marker files no longer count.
    remove((directory + "/frame-stats").c_str());
    BrowserSettingsLoad(directory.c_str());
    assert(BrowserSetting("frame-stats") && !BrowserSetting("gpu-tiles"));

    write(ini, "; comment\r\n[debug]\r\nFrame-Stats = OFF  # trailing\r\n  js-tier=LLInt\r\n"
        "jit-priority = yes\ntimer-deferral=0\nbogus = 1\njs-tier = fast\nnovalue\n");
    lines.clear();
    BrowserSettingsLoad(directory.c_str());
    assert(!BrowserSetting("frame-stats") && BrowserSetting("jit-priority") && !BrowserSetting("timer-deferral"));
    assert(BrowserSetting("gpu-tiles"));
    assert(std::string(BrowserSettingText("js-tier")) == "llint");
    assert(!BrowserSetting("no-such-setting") && std::string(BrowserSettingText("no-such-setting")).empty());
    BrowserSettingsDescribe(collect);
    assert(lines.size() == 4);
    assert(lines[0] == "settings: timer-deferral=off jit-priority=on js-tier=llint");
    assert(lines[1] == "settings: unknown name 'bogus'");
    assert(lines[2] == "settings: bad value for 'js-tier'");
    assert(lines[3] == "settings: no value in 'novalue'");

    write(ini, "");
    lines.clear();
    BrowserSettingsLoad(directory.c_str());
    BrowserSettingsDescribe(collect);
    assert(lines.size() == 1 && lines[0] == "settings: all at their defaults");
    puts("PASS: webkit.ini defaults, marker files, parsing, problems");
    return 0;
}
