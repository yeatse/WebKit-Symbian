PHDRS { code PT_LOAD FLAGS(5); dynamic PT_DYNAMIC FLAGS(4); }
SECTIONS {
 . = SIZEOF_HEADERS;
 .text : { *(ER_RO) } :code
 .dynsym : { *(.dynsym) } :code
 .gnu.version : { *(.gnu.version) } :code
 .gnu.version_d : { *(.gnu.version_d) } :code
 .hash : { *(.hash) } :code
 .dynstr : { *(.dynstr) } :code
 .dynamic : { *(.dynamic) } :code :dynamic
 /DISCARD/ : { *(.comment) *(.ARM.attributes) }
}
