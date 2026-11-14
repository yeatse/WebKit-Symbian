# ROM probe

Names what the browser's `profile` setting finds outside the image. On the
phone the samples in system code are ROM addresses with no symbols; this
program prints the instructions at those addresses, the code of the library
functions the browser imports for copying, locking and allocating, and the
time of one call of each.

    SYMBIAN_SDK_PREFIX=... ./build.sh

Install `romprobe.sis`, run `romprobe` in fshell (it shows no window and exits
by itself after a few seconds) and read `C:\Data\romprobe.txt`.

The sampled addresses are built in, from the profile of one build on the 808.
For others, put one hexadecimal address per line in
`C:\Data\romprobe-in.txt` before starting it. Only addresses the profiler
reported are safe to list: reading an unmapped address ends the program.
