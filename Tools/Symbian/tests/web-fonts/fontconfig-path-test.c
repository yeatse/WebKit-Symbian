#include <fontconfig/fontconfig.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int failures;
static void check(int ok, const char* name)
{
    printf("FONTCONFIG %s %s\n", ok ? "PASS" : "FAIL", name);
    failures += !ok;
}

static void canonical(const char* path, const char* expected)
{
    FcChar8* result = FcStrCopyFilename((const FcChar8*)path);
    check(result && !strcmp((const char*)result, expected), path);
    if (result)
        FcStrFree(result);
}

int main(int argc, char** argv)
{
    if (argc != 2)
        return 2;
    canonical("Z:/resource/fonts", "Z:/resource/fonts");
    canonical("C:\\resource\\fontconfig\\fonts.conf", "C:/resource/fontconfig/fonts.conf");
    canonical("E:/fonts//test/../Ahem.ttf", "E:/fonts/Ahem.ttf");
    char cwd[4096], expected[4200];
    if (!getcwd(cwd, sizeof(cwd)))
        return 2;
    snprintf(expected, sizeof(expected), "%s/fonts/Ahem.ttf", cwd);
    canonical("fonts\\Ahem.ttf", expected);

    FcConfig* config = FcConfigCreate();
    check(config != NULL, "create configuration");
    if (!config)
        return 1;
    check(FcConfigParseAndLoad(config, (const FcChar8*)argv[1], FcTrue), "parse shipped fonts.conf");
    FcStrList* dirs = FcConfigGetFontDirs(config);
    FcChar8* first = dirs ? FcStrListNext(dirs) : NULL;
    check(first && !strcmp((const char*)first, "Z:/resource/fonts"), "ROM font directory preserves drive");
    FcChar8* second = dirs ? FcStrListNext(dirs) : NULL;
    check(second && !strcmp((const char*)second, "C:/resource/fonts"), "writable font directory preserves drive");
    check(dirs && !FcStrListNext(dirs), "no host or HOME font directories");
    if (dirs)
        FcStrListDone(dirs);
    dirs = FcConfigGetCacheDirs(config);
    first = dirs ? FcStrListNext(dirs) : NULL;
    check(first && !strcmp((const char*)first, "C:/Data/fontconfig-cache"), "writable cache directory");
    if (dirs)
        FcStrListDone(dirs);
    setenv("FONTCONFIG_PATH", "D:/overrides;C:/resource/fontconfig", 1);
    FcChar8* path = FcConfigGetFilename(config, (const FcChar8*)"override.conf");
    check(path && !strcmp((const char*)path, "D:/overrides/override.conf"), "semicolon search path keeps drive colon");
    if (path)
        FcStrFree(path);
    path = FcConfigGetFilename(config, (const FcChar8*)"D:/overrides/override.conf");
    check(path && !strcmp((const char*)path, "D:/overrides/override.conf"), "absolute configuration path keeps drive");
    if (path)
        FcStrFree(path);
    setenv("FONTCONFIG_FILE", "D:/overrides/override.conf", 1);
    path = FcConfigFilename(NULL);
    check(path && !strcmp((const char*)path, "D:/overrides/override.conf"), "absolute FONTCONFIG_FILE keeps drive");
    if (path)
        FcStrFree(path);
    FcConfigDestroy(config);
    printf("FONTCONFIG RESULT %d failures\n", failures);
    return failures ? 1 : 0;
}
