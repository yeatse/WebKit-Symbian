#include "CommandLine.h"
#include <cassert>
#include <cstring>
#include <cstdio>
int main()
{
    char line[] = "audio file \"C:\\Data\\WebKit\\with space.wav\" 10000 --timeout-ms 30000";
    char* argv[32];
    assert(parseNativeArguments(line, argv, 32) == 7);
    assert(!strcmp(argv[1], "audio") && !strcmp(argv[2], "file"));
    assert(!strcmp(argv[3], "C:\\Data\\WebKit\\with space.wav"));
    assert(!strcmp(argv[6], "30000") && !argv[7]);
    char url[] = "\tvideo\turl 'http://localhost:8775/video.mp4?x=1&y=2' --surface-events ";
    assert(parseNativeArguments(url, argv, 32) == 5);
    assert(!strcmp(argv[3], "http://localhost:8775/video.mp4?x=1&y=2"));
    char unclosed[] = "audio file \"broken";
    assert(parseNativeArguments(unclosed, argv, 32) == -1);
    char tooMany[] = "audio file test.wav";
    assert(parseNativeArguments(tooMany, argv, 4) == -1);
    char empty[] = " \t";
    assert(parseNativeArguments(empty, argv, 32) == 1 && !argv[1]);
    char emptyArgument[] = "audio file ''";
    assert(parseNativeArguments(emptyArgument, argv, 32) == 4 && !*argv[3]);
    std::puts("PASS: native argument parser preserves quoted paths/URLs and rejects malformed/overflow input");
}
