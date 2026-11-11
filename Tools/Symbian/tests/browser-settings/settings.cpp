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
    assert(!BrowserSetting("backdrop-filters") && BrowserSetting("jit-priority") && !BrowserSetting("curl-verbose"));
    assert(std::string(BrowserSettingText("js-tier")) == "dfg");
    std::string written = read(ini);
    assert(written.find("frame-stats = on\r\n") != std::string::npos);
    assert(written.find("gpu-tiles = off\r\n") != std::string::npos);
    assert(written.find("\r\n#js-tier = dfg\r\n") != std::string::npos);
    assert(written.find("\r\n#jit-priority = on\r\n") != std::string::npos);
    assert(written.find("\r\n#js-options = \r\n") != std::string::npos);
    assert(std::string(BrowserSettingText("js-options")).empty());
    BrowserSettingsDescribe(collect);
    assert(lines.size() == 1 && lines[0] == "settings: frame-stats=on gpu-tiles=off");

    // The file written reads back the same; marker files no longer count.
    remove((directory + "/frame-stats").c_str());
    BrowserSettingsLoad(directory.c_str());
    assert(BrowserSetting("frame-stats") && !BrowserSetting("gpu-tiles"));

    write(ini, "; comment\r\n[debug]\r\nFrame-Stats = OFF  # trailing\r\n  js-tier=LLInt\r\n"
        "jit-priority = no\ntimer-deferral=0\nbogus = 1\njs-tier = fast\nnovalue\n");
    lines.clear();
    BrowserSettingsLoad(directory.c_str());
    assert(!BrowserSetting("frame-stats") && !BrowserSetting("jit-priority") && !BrowserSetting("timer-deferral"));
    assert(BrowserSetting("gpu-tiles"));
    assert(std::string(BrowserSettingText("js-tier")) == "llint");
    assert(!BrowserSetting("no-such-setting") && std::string(BrowserSettingText("no-such-setting")).empty());
    BrowserSettingsDescribe(collect);
    assert(lines.size() == 4);
    assert(lines[0] == "settings: timer-deferral=off jit-priority=off js-tier=llint");
    assert(lines[1] == "settings: unknown name 'bogus'");
    assert(lines[2] == "settings: bad value for 'js-tier'");
    assert(lines[3] == "settings: no value in 'novalue'");

    // As the phone's editors save it: UTF-16 with a byte order mark.
    for (bool littleEndian : { true, false }) {
        std::string ascii = "# \xe9 note\r\nframe-stats = on\r\njs-tier = baseline\r\n";
        std::string wide = littleEndian ? "\xff\xfe" : "\xfe\xff";
        for (unsigned char character : ascii) {
            // U+4E2D for the comment's letter: neither byte is syntax.
            char low = character == 0xe9 ? 0x2d : character;
            char high = character == 0xe9 ? 0x4e : 0;
            wide += littleEndian ? low : high;
            wide += littleEndian ? high : low;
        }
        FILE* file = fopen(ini.c_str(), "wb");
        fwrite(wide.data(), 1, wide.size(), file);
        fclose(file);
        lines.clear();
        BrowserSettingsLoad(directory.c_str());
        BrowserSettingsDescribe(collect);
        assert(lines.size() == 1 && lines[0] == "settings: frame-stats=on js-tier=baseline");
    }
    write(ini, "\xef\xbb\xbfjit-priority = off\n");
    lines.clear();
    BrowserSettingsLoad(directory.c_str());
    BrowserSettingsDescribe(collect);
    assert(lines.size() == 1 && lines[0] == "settings: jit-priority=off");

    // js-options keeps its case and reaches the caller pair by pair.
    write(ini, "JS-Options = thresholdForOptimizeAfterWarmUp=2000  useDFGJIT=false,bogus =x\n");
    lines.clear();
    BrowserSettingsLoad(directory.c_str());
    assert(std::string(BrowserSettingText("js-options")) == "thresholdForOptimizeAfterWarmUp=2000  useDFGJIT=false,bogus =x");
    static std::vector<std::string> options;
    BrowserSettingsForEachJSOption([](const char* name, const char* value) {
        options.push_back(std::string(name) + "|" + value);
    });
    assert(options.size() == 2 && options[0] == "thresholdForOptimizeAfterWarmUp|2000" && options[1] == "useDFGJIT|false");
    BrowserSettingsDescribe(collect);
    assert(lines.size() == 4);
    assert(lines[0] == "settings:");
    assert(lines[1] == "settings: js-options = thresholdForOptimizeAfterWarmUp=2000  useDFGJIT=false,bogus =x");
    assert(lines[2] == "settings: js-options needs name=value, not 'bogus'");
    assert(lines[3] == "settings: js-options needs name=value, not '=x'");
    write(ini, ("js-options = " + std::string(200, 'a') + "\n").c_str());
    lines.clear();
    BrowserSettingsLoad(directory.c_str());
    assert(std::string(BrowserSettingText("js-options")).empty());

    write(ini, "");
    lines.clear();
    BrowserSettingsLoad(directory.c_str());
    BrowserSettingsDescribe(collect);
    assert(lines.size() == 1 && lines[0] == "settings: all at their defaults");
    puts("PASS: webkit.ini defaults, marker files, parsing, UTF-16 and UTF-8 marks, problems");
    return 0;
}
