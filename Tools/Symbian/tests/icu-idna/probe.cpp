#include <unicode/uclean.h>
#include <unicode/udata.h>
#include <unicode/uidna.h>

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

int main(int argc, char** argv)
{
    if (argc != 2 && (argc != 3 || std::strcmp(argv[2], "--expect-missing")))
        return 2;
    bool expectMissing = argc == 3;
    std::ifstream file(argv[1], std::ios::binary | std::ios::ate);
    if (!file || file.tellg() <= 0)
        return 2;
    auto size = file.tellg();
    // ICU data must be aligned and must outlive all users, including cleanup.
    std::vector<std::max_align_t> data((static_cast<size_t>(size) + sizeof(std::max_align_t) - 1) / sizeof(std::max_align_t));
    file.seekg(0);
    if (!file.read(reinterpret_cast<char*>(data.data()), size))
        return 2;
    UErrorCode error = U_ZERO_ERROR;
    // No file-system fallback. The runner links empty stubdata, never the full
    // host libicudata.a, so this exact package is the only available data.
    udata_setFileAccess(UDATA_NO_FILES, &error);
    udata_setCommonData(data.data(), &error);
    if (U_FAILURE(error)) {
        std::printf("DATA FAIL %s\n", u_errorName(error));
        return 2;
    }
    auto* idna = uidna_openUTS46(UIDNA_CHECK_BIDI | UIDNA_CHECK_CONTEXTJ
        | UIDNA_NONTRANSITIONAL_TO_UNICODE | UIDNA_NONTRANSITIONAL_TO_ASCII, &error);
    std::printf("uidna_openUTS46=%s (%d), pointer=%s\n", u_errorName(error), int(error), idna ? "non-null" : "null");
    if (expectMissing) {
        bool passed = !idna && error == U_FILE_ACCESS_ERROR;
        if (idna)
            uidna_close(idna);
        u_cleanup();
        return passed ? 0 : 1;
    }
    if (!idna || U_FAILURE(error))
        return 1;
    struct Case {
        const char* name;
        const char* input;
        const char* output;
        uint32_t requiredError;
    } cases[] = {
        { "ASCII", "EXAMPLE.com", "example.com", 0 },
        { "Latin IDN", u8"b\u00fccher.example", "xn--bcher-kva.example", 0 },
        { "Chinese IDN", u8"\u4f8b\u5b50.\u6d4b\u8bd5", "xn--fsqu00a.xn--0zwm56d", 0 },
        { "punycode", "xn--bcher-kva.example", "xn--bcher-kva.example", 0 },
        { "nontransitional", u8"fa\u00df.de", "xn--fa-hia.de", 0 },
        { "width mapping", u8"\uff25\uff38\uff21\uff2d\uff30\uff2c\uff25\uff0ecom", "example.com", 0 },
        { "ContextJ rejection", u8"ab\u200dcd.example", nullptr, UIDNA_ERROR_CONTEXTJ },
        { "bidi rejection", u8"\u05d0a.example", nullptr, UIDNA_ERROR_BIDI },
        { "punycode rejection", "xn--.example", nullptr, UIDNA_ERROR_INVALID_ACE_LABEL },
    };
    unsigned failed = 0;
    for (auto& test : cases) {
        char output[256];
        UIDNAInfo info = UIDNA_INFO_INITIALIZER;
        error = U_ZERO_ERROR;
        int32_t length = uidna_nameToASCII_UTF8(idna, test.input, -1, output, sizeof(output), &info, &error);
        bool passed = U_SUCCESS(error) && (test.requiredError
            ? (info.errors & test.requiredError) == test.requiredError
            : !info.errors && length >= 0 && std::string(output, static_cast<size_t>(length)) == test.output);
        std::printf("%s %s status=%s errors=0x%x\n", passed ? "PASS" : "FAIL", test.name, u_errorName(error), info.errors);
        failed += !passed;
    }
    uidna_close(idna);
    u_cleanup();
    return failed ? 1 : 0;
}
