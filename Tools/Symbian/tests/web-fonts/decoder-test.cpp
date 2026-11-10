#include <woff2/decode.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

static int failures;
static void check(bool ok, const char* name)
{
    std::printf("WEBFONT %s %s\n", ok ? "PASS" : "FAIL", name);
    failures += !ok;
}

static std::vector<unsigned char> read(const std::string& path)
{
    std::ifstream input(path, std::ios::binary);
    return { std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() };
}

static bool loadFont(FT_Library library, const std::vector<unsigned char>& bytes)
{
    FT_Face face = nullptr;
    if (bytes.empty() || FT_New_Memory_Face(library, bytes.data(), bytes.size(), 0, &face))
        return false;
    bool valid = face->family_name && std::string(face->family_name) == "Ahem"
        && !FT_Load_Char(face, 'X', FT_LOAD_NO_SCALE)
        && face->glyph->advance.x == face->units_per_EM;
    FT_Done_Face(face);
    return valid;
}

int main(int argc, char** argv)
{
    if (argc != 2)
        return 2;
    std::string directory = argv[1];
    FT_Library library = nullptr;
    if (FT_Init_FreeType(&library))
        return 2;
    check(loadFont(library, read(directory + "/Ahem.ttf")), "static TTF metrics");
    for (const char* name : { "Ahem.woff2", "truncated.woff2", "invalid-flavor.woff2" }) {
        auto input = read(directory + "/" + name);
        check(!input.empty(), name);
        size_t size = woff2::ComputeWOFF2FinalSize(input.data(), input.size());
        std::vector<unsigned char> output(size <= 1024 * 1024 ? size : 0);
        bool decoded = !output.empty() && woff2::ConvertWOFF2ToTTF(output.data(), output.size(), input.data(), input.size());
        bool loaded = decoded && loadFont(library, output);
        check(std::string(name) == "Ahem.woff2" ? loaded : !loaded, name);
    }
    FT_Done_FreeType(library);
    std::printf("WEBFONT RESULT %d failures\n", failures);
    return failures ? 1 : 0;
}
