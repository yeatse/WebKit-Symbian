# JPEG decoding: ICL and libjpeg

Answers whether the phone has a hardware JPEG decoder worth using for web
images, and how it compares with the libjpeg decoding WebKit does now.

```sh
export SYMBIAN_SDK_PREFIX=/path/to/sdk
Tools/Symbian/tests/jpeg-decode-probe/build.sh
```

Install `jpegprobe.sis` and run `jpegprobe` in fshell. It shows no window and exits by itself within about
three minutes; an ICL decode that runs over 15 s stops the probe. The results
are in `C:\Data\jpegprobe.txt`:

- every ICL image decoder plug-in, JPEG ones marked `*`, with their ECom data
  and plug-in property UIDs, which is where a hardware decoder shows itself;
- for baseline and progressive JPEGs of 170x96, 640x360, 1280x720 and
  2048x1536 (encoded on the phone, quality 85), the best and mean time of:
  - libjpeg as WebKit uses it (ISLOW IDCT, fancy upsampling, 32-bit pixels),
  - libjpeg with the fast IDCT and plain upsampling,
  - libjpeg with 1/2 and 1/4 DCT scaling,
  - ICL's default JPEG decoder at full and half size, with the time to create
    the decoder apart from the conversion,
  - each JPEG plug-in chosen explicitly.

Up to six photos of up to 8 megapixels in `C:\Data\jpegprobe\*.jpg` are
timed first, the same way.
