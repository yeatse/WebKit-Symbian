# Exercise the production ARM backend independently of JSC's runtime/layout.
# These leaf wrappers expose its operations through the AAPCS softfp ABI.
if ARM
    macro enter()
        emit "push {r4-r12, lr}"
    end
    macro leave()
        emit "pop {r4-r12, lr}"
        ret
    end

    global _offline_tzcnt
_offline_tzcnt:
    enter()
    tzcnti t0, t0
    leave()

    global _offline_constants
_offline_constants:
    enter()
    move 0x12345678, t1
    storei t1, 0[t0]
    move -536870913, t1
    storei t1, 4[t0]
    move 0x80000000, t1
    storei t1, 8[t0]
    move 0x7fffffff, t1
    storei t1, 12[t0]
    move -1, t1
    storei t1, 16[t0]
    move 0x00ff00ff, t1
    storei t1, 20[t0]
    move 256, t1
    storei t1, 24[t0]
    move 0, t1
    storei t1, 28[t0]
    leave()

    global _offline_symbol
_offline_symbol:
    enter()
    leap _probe_global_data, t0
    leave()

    global _offline_label_delta
_offline_label_delta:
    enter()
_offline_delta_begin:
    nop
    nop
_offline_delta_end:
    mvlbl (_offline_delta_end - _offline_delta_begin), t0
    leave()

    global _offline_signed_less
_offline_signed_less:
    enter()
    cilt t0, t1, t0
    leave()

    global _offline_unsigned_less
_offline_unsigned_less:
    enter()
    cib t0, t1, t1
    move t1, t0
    leave()

    global _offline_equal
_offline_equal:
    enter()
    cieq t0, t1, t0
    leave()

    global _offline_double_mask
_offline_double_mask:
    enter()
    # Base PCS passes both doubles in core pairs, not d0/d1.
    fii2d t0, t1, ft0
    fii2d t2, t3, ft1
    cdeq ft0, ft1, t0
    cdnequn ft0, ft1, t1
    lshifti 1, t1
    ori t1, t0
    cdlt ft0, ft1, t1
    lshifti 2, t1
    ori t1, t0
    cdlteq ft0, ft1, t1
    lshifti 3, t1
    ori t1, t0
    cdgt ft0, ft1, t1
    lshifti 4, t1
    ori t1, t0
    cdgteq ft0, ft1, t1
    lshifti 5, t1
    ori t1, t0
    leave()

    global _offline_load_positive
_offline_load_positive:
    enter()
    loadi 4096[t0], t0
    leave()

    global _offline_load_negative
_offline_load_negative:
    enter()
    loadi -4096[t0], t0
    leave()

    global _offline_load_half
_offline_load_half:
    enter()
    loadh 300[t0, t1, 2], t0
    leave()

    global _offline_load_signed_half
_offline_load_signed_half:
    enter()
    loadhsi -300[t0, t1, 2], t0
    leave()

    global _offline_load_signed_byte
_offline_load_signed_byte:
    enter()
    loadbsi -300[t0, t1, 1], t0
    leave()

    global _offline_store_half
_offline_store_half:
    enter()
    storeh t2, 300[t0, t1, 2]
    leave()

    global _offline_pair_first_alias
_offline_pair_first_alias:
    enter()
    load2ia 0[t0], t0, t3
    move t3, t1
    leave()

    global _offline_pair_second_alias
_offline_pair_second_alias:
    enter()
    load2ia 0[t0], t2, t0
    move t0, t1
    move t2, t0
    leave()

    global _offline_pair_scaled
_offline_pair_scaled:
    enter()
    load2ia 16[t0, t1, 4], t0, t3
    move t3, t1
    leave()

    global _offline_store_pair
_offline_store_pair:
    enter()
    store2ia t1, t2, 12[t0]
    leave()

    global _offline_store_pair_alias
_offline_store_pair_alias:
    enter()
    store2ia t0, t1, 0[t0]
    leave()
else
    error
end
