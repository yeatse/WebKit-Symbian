/* The SDK's exe_unwind_image.ld (exception unwinding for the AVKON shell)
   rearranged for a large image:
   - the PLT comes first, as in e32/jsc_image.ld, then the Thumb code (the
     SDK runtime and its mbedTLS, FreeType, libpng, libjpeg and zlib, the
     startup code and the shell, compiled by the SDK's Thumb toolchain), so
     that calls into OS libraries mostly reach the PLT directly;
   - calls beyond a BL's reach (+-32 MB for ARM, +-4 MB for Thumb) go
     through --pic-veneer thunks, which the patched converter follows to
     the PLT. The image is not limited to 32 MB of code. */
ENTRY(_E32Startup)
PHDRS { code PT_LOAD FLAGS(5); data PT_LOAD FLAGS(6); dynamic PT_DYNAMIC FLAGS(4); }
SECTIONS {
 . = 0x8000;
 __ro_base = .;
 .plt : { *(.plt) } :code
 .text : {
   KEEP(*(.text.startup))
   */browser.dir/*(.text*)
   *libsymbian_*.a:*(.text*)
   *libmbed*.a:*(.text*)
   *(.text*)
 } :code
 .rodata : { KEEP(*(.rodata.symbian_exception_descriptor)) *(.rodata*) } :code
 .ARM.extab : { *(.ARM.extab*) } :code
 .ARM.exidx : { __exidx_start = .; *(.ARM.exidx*) __exidx_end = .; } :code
 .init_array : { HIDDEN(__init_array_start = .); KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*))) KEEP(*(.init_array)) HIDDEN(__init_array_end = .); } :code
 .fini_array : { HIDDEN(__fini_array_start = .); KEEP(*(SORT_BY_INIT_PRIORITY(.fini_array.*))) KEEP(*(.fini_array)) HIDDEN(__fini_array_end = .); } :code
 .dynsym : { *(.dynsym) } :code
 .dynstr : { *(.dynstr) } :code
 .gnu.version : { *(.gnu.version) } :code
 .gnu.version_r : { *(.gnu.version_r) } :code
 .hash : { *(.hash) } :code
 .rel.dyn : { *(.rel.dyn) } :code
 .rel.plt : { *(.rel.plt) } :code
 .got.plt : { *(.got.plt) } :code
 .got : { *(.got) } :code
 .dynamic : { *(.dynamic) } :code :dynamic
 __ro_end = .;
 . = 0x20000000;
 .data : { *(.data .data.*) . = ALIGN(4); } :data
 .bss (NOLOAD) : { *(.bss .bss.* COMMON) . = ALIGN(4); } :data
 /DISCARD/ : { *(.comment) }
}
