/* Freestanding Linux ARM harness: only write/exit syscalls, no target libc. */
typedef unsigned int u32;
typedef signed int i32;
typedef unsigned long long u64;
_Static_assert(sizeof(void*) == 4 && sizeof(u32) == 4, "32-bit ARM required");

extern u32 offline_tzcnt(u32);
extern void offline_constants(u32*);
extern void* offline_symbol(void);
extern u32 offline_label_delta(void);
extern u32 offline_signed_less(u32, u32);
extern u32 offline_unsigned_less(u32, u32);
extern u32 offline_equal(u32, u32);
extern u32 offline_double_mask(double, double);
extern u32 offline_load_positive(const void*);
extern u32 offline_load_negative(const void*);
extern u32 offline_load_half(const void*, u32);
extern u32 offline_load_signed_half(const void*, u32);
extern u32 offline_load_signed_byte(const void*, u32);
extern void offline_store_half(void*, u32, u32);
extern u64 offline_pair_first_alias(const void*);
extern u64 offline_pair_second_alias(const void*);
extern u64 offline_pair_scaled(const void*, u32);
extern void offline_store_pair(void*, u32, u32);
extern void offline_store_pair_alias(void*, u32);

u32 probe_global_data = 0xfeedface;
static unsigned failures;
static unsigned checks;

/* GCC may lower aggregate initialization to memset even in freestanding C. */
void* memset(void* destination, int value, unsigned bytes)
{
    volatile unsigned char* out = destination;
    for (unsigned i = 0; i < bytes; ++i)
        out[i] = (unsigned char)value;
    return destination;
}

static void output(const char* text)
{
    unsigned length = 0;
    while (text[length])
        ++length;
    register u32 arg0 __asm__("r0") = 1;
    register const char* arg1 __asm__("r1") = text;
    register u32 arg2 __asm__("r2") = length;
    register u32 syscall __asm__("r7") = 4;
    __asm__ volatile("svc #0" : "+r"(arg0) : "r"(arg1), "r"(arg2), "r"(syscall) : "memory");
}

static void hex(u32 value)
{
    char result[11] = "0x00000000";
    for (unsigned i = 0; i < 8; ++i)
        result[9 - i] = "0123456789abcdef"[(value >> (i * 4)) & 15];
    output(result);
}

static void equal(const char* test, u32 actual, u32 expected)
{
    ++checks;
    if (actual == expected)
        return;
    ++failures;
    output("FAIL: "); output(test); output(" got="); hex(actual);
    output(" expected="); hex(expected); output("\n");
}

static void equal_pair(const char* test, u64 actual, u32 low, u32 high)
{
    equal(test, (u32)actual, low);
    equal(test, (u32)(actual >> 32), high);
}

static u32 random_word(u32* state)
{
    u32 value = *state;
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    return *state = value;
}

static unsigned trailing_zeros(u32 value)
{
    for (unsigned bit = 0; bit < 32; ++bit) {
        if ((value >> bit) & 1)
            return bit;
    }
    return 32;
}

static void counts(void)
{
    unsigned before = failures;
    equal("tzcnt zero", offline_tzcnt(0), 32);
    equal("tzcnt all bits", offline_tzcnt(~0u), 0);
    for (unsigned bit = 0; bit < 32; ++bit)
        equal("tzcnt single bit", offline_tzcnt(1u << bit), bit);
    u32 state = 0x12345678;
    for (unsigned i = 0; i < 2048; ++i) {
        u32 value = random_word(&state);
        equal("tzcnt random", offline_tzcnt(value), trailing_zeros(value));
    }
    if (failures == before) output("PASS: in-place tzcnti including zero\n");
}

static void constants(void)
{
    unsigned before = failures;
    u32 values[8] = { 0 };
    static const u32 expected[8] = { 0x12345678, 0xdfffffff, 0x80000000, 0x7fffffff, 0xffffffff, 0x00ff00ff, 256, 0 };
    offline_constants(values);
    for (unsigned i = 0; i < 8; ++i)
        equal("constant", values[i], expected[i]);
    equal("absolute symbol literal", (u32)offline_symbol(), (u32)&probe_global_data);
    equal("relative label literal", offline_label_delta(), 8);
    if (failures == before) output("PASS: constants and symbolic literals\n");
}

static void compare(u32 a, u32 b)
{
    equal("signed comparison with left alias", offline_signed_less(a, b), (i32)a < (i32)b);
    equal("unsigned comparison with right alias", offline_unsigned_less(a, b), a < b);
    equal("equality with left alias", offline_equal(a, b), a == b);
}

static void integers(void)
{
    unsigned before = failures;
    static const u32 values[] = { 0, 1, 255, 256, 0x7fffffff, 0x80000000, 0xfffffffe, 0xffffffff };
    for (unsigned i = 0; i < sizeof(values) / sizeof(values[0]); ++i)
        for (unsigned j = 0; j < sizeof(values) / sizeof(values[0]); ++j)
            compare(values[i], values[j]);
    u32 state = 0xfedcba98;
    for (unsigned i = 0; i < 2048; ++i) {
        u32 a = random_word(&state);
        u32 b = random_word(&state);
        compare(a, b);
    }
    if (failures == before) output("PASS: signed unsigned and equality comparisons\n");
}

static double as_double(u32 high, u32 low)
{
    union { double number; u32 words[2]; } value;
    value.words[0] = low;
    value.words[1] = high;
    return value.number;
}

static void doubles(void)
{
    unsigned before = failures;
    /* eq/ne-unordered/lt/le/gt/ge occupy bits 0..5. */
    equal("double less", offline_double_mask(1.0, 2.0), 14);
    equal("double greater", offline_double_mask(2.0, 1.0), 50);
    equal("double negative", offline_double_mask(-2.0, -1.0), 14);
    equal("double signed zero", offline_double_mask(0.0, -0.0), 41);
    equal("double equal", offline_double_mask(1.5, 1.5), 41);
    double infinity = as_double(0x7ff00000, 0);
    double nan = as_double(0x7ff80000, 1);
    double tiny = as_double(0, 1);
    equal("double infinity", offline_double_mask(1.0, infinity), 14);
    equal("double infinity equal", offline_double_mask(infinity, infinity), 41);
    equal("double left NaN", offline_double_mask(nan, 1.0), 2);
    equal("double right NaN", offline_double_mask(1.0, nan), 2);
    equal("double two NaNs", offline_double_mask(nan, nan), 2);
    equal("double subnormal", offline_double_mask(tiny, 0.0), 50);
    if (failures == before) output("PASS: VFPv2 comparisons NaN infinity and signed zero\n");
}

static u32 memory[3072];
static void addresses(void)
{
    unsigned before = failures;
    unsigned char* center = (unsigned char*)(memory + 1024);
    memory[0] = 0x89abcdef;
    memory[2048] = 0x76543210;
    equal("positive large offset", offline_load_positive(center), memory[2048]);
    equal("negative large offset", offline_load_negative(center), memory[0]);
    center[306] = 0x34; center[307] = 0xab;
    equal("halfword scaled base alias", offline_load_half(center, 3), 0xab34);
    center[-294] = 0x23; center[-293] = 0x81;
    equal("signed halfword negative base alias", offline_load_signed_half(center, 3), 0xffff8123);
    center[-297] = 0x81;
    equal("signed byte negative base alias", offline_load_signed_byte(center, 3), 0xffffff81);
    center[305] = 0x55; center[308] = 0xaa;
    offline_store_half(center, 3, 0x1234fedc);
    equal("halfword store low", center[306], 0xdc);
    equal("halfword store high", center[307], 0xfe);
    equal("halfword store left guard", center[305], 0x55);
    equal("halfword store right guard", center[308], 0xaa);
    if (failures == before) output("PASS: large signed offsets and halfword base aliases\n");
}

static void pairs(void)
{
    unsigned before = failures;
    u32 words[16] = { 0x01234567, 0x89abcdef };
    equal_pair("pair first aliases base", offline_pair_first_alias(words), words[0], words[1]);
    equal_pair("pair second aliases base", offline_pair_second_alias(words), words[0], words[1]);
    words[7] = 0xfedcba98; words[8] = 0x76543210;
    equal_pair("pair scaled base alias", offline_pair_scaled(words, 3), words[7], words[8]);
    words[2] = 0xaa55aa55; words[5] = 0x55aa55aa;
    offline_store_pair(words, 0x2468ace0, 0x13579bdf);
    equal("pair store low", words[3], 0x2468ace0);
    equal("pair store high", words[4], 0x13579bdf);
    equal("pair store left guard", words[2], 0xaa55aa55);
    equal("pair store right guard", words[5], 0x55aa55aa);
    offline_store_pair_alias(words, 0xdeadbeef);
    equal("pair store base is source", words[0], (u32)words);
    equal("pair store alias high", words[1], 0xdeadbeef);
    if (failures == before) output("PASS: arbitrary register pairs and base aliases\n");
}

int probe_main(void)
{
    counts(); constants(); integers(); doubles(); addresses(); pairs();
    if (failures)
        return 1;
    output("PASS: arm11-offlineasm checks="); hex(checks); output("\n");
    return 0;
}
