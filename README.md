brawshot
========

This tool processes videos in Blacmagic RAW format and applies a temporal
lowpass filter to significantly reduce noise for videos recorded in low light
environments. Essentially this results in a virtual "very long exposure time"
for video.


The Algorithm
-------------

A digital image sensor records images with noise. This noise, like most random
processes in nature, is roughly Gaussian distributed. Statistics tells us that
computing the mean of a large number of Gaussian distributed samples eventually
results in the true mean of the underlying Gaussian distribution.

When you use a long exposure time on a standard photo camera, something very
similar happens internally: light is collected over an extended period of time
to significantly reduce the image sensor noise.

This tool computes a moving average of successive video frames and therefore
mostly reduces the random image sensor noise. Unfortunately this has two
drawbacks:

- There must not be any fast movements, since this would result in extreme
  motion blur
- Pattern noise and dead pixels of the image sensor will *not* be removed

However, the noise reduced images are "good enough" that applying extra gain
becomes feasible to improve brightness significantly beyond what's reasonable
without this noise reduction in place.

The algorithm could be implemented on a CPU and the first prototype was in fact
purely CPU based, but that is extremely slow. The implementation provided in
this repository is GPU accelerated via OpenGL.


OpenGL Implementation Details
-----------------------------

The BRAW SDK can output video frames in various different formats, including:

- 8bit per channel (unsigned int)
- 16bit per channel (unsigned int)
- 32bit per channel (float)

Apparently a Blackmagic Design camera records in 12bit and the relevant videos
are recorded in low light environments, so only 16bit uint or 32bit float make
sense. Since an efficient implementation of the moving average requires that

```
add_frame(frame_0)
add_frame(frame_1)
...
add_frame(frame_99)
remove_frame(frame_0)
remove_frame(frame_1)
...
remove_frame(frame_98)
output()
```

results in the bit identical data as

```
add_frame(frame_99)
output()
```

it is necessary to use an integer accumulation buffer. Floating point would
inevitably result in rounding errors which are unacceptable here. In OpenGL a
32bit uint texture is used for the render buffer and the BRAW decoder is
instructed to decode to 16bit uint frames.

The GLSL shader then accumulates the frames into the 32bit uint texture, where
`add_frame` adds the frame to the buffer and `remove_frame` subtracts the frame
from the buffer.

After the buffer is filled with frames according to the window size, the
`output` shader divides the sum by the sample count, multiplies by the
specified gain factor, and optionally applies a 3D LUT.

The result is a noise reduced standard 8bit per channel image.

As a result of the 32bit accumulation buffer and 16bit frames, you should not
attempt to sum up more than 65535 frames / set a window size larger than 65535
frames. In practice, you should never get close to this limit anyway.


Why?
----

Because I'm crazy and I wanted to record the aurora on 11.05.2024 but I only
have a Blackmagic Design Pocket Cinema Camera 6k Pro which is clearly not made
for this purpose.


Why JPEG as Output Format?
--------------------------

Because as it turns out, writing uncompressed 8bit images results in around
60MB _per frame_ for 6k video. Alternatively, 16bit images result twice that
size. In both cases, huge amounts of storage are necessary and disk I/O speed
becomes the bottleneck. JPEG compression via libjpeg-turbo is so fast that it
results in a significant speedup without any noticeable loss of quality. Even
with JPEG compression a 2.2 GB BRAW video still results in 3.3 GB JPEG files for
one video I processed with this tool.


System Requirements
-------------------

- Linux/AMD64
- Blackmagic RAW SDK
- gcc (g++)
- make
- [bin2o](https://github.com/hackyourlife/bin2o)
- glslang
- libjpeg-turbo
- EGL
- Graphics card with support for modern OpenGL and NVIDIA style headless EGL
- A Blackmagic Design camera which records BRAW files
- Optionally: a 3D LUT file for your camera


Building
--------

Run `make` to compile the tool. If the BRAW SDK is installed in a different
location than the one chosen by
[blackmagic-raw-sdk](https://aur.archlinux.org/packages/blackmagic-raw-sdk) you
have to adjust the path in the Makefile and in src/main.cpp.


Usage
-----

Record a video with the Blackmagic video camera in BRAW format and with the
maximum ISO setting, even if you can barely see anything on the camera's screen
because of all the noise. Then run brawshot to process the video:

```sh
brawshot -i input.braw -o output [-l lut.cube] [-g 2.0] [-w 100]
```

The options have the following meaning:
- `-i input.braw`: the input file in BRAW format
- `-o output`: output file name prefix. With `-o output` the result will be a
  series of images with the first frame being `output-0000.jpg`. Do _not_
  attempt to use a `%` sign in the output file name, this _will_ break things.
- `-l lut.cube`: 3D LUT to convert from the camera's color space to something
  more useful
- `-g 2.0`: gain factor applied after computing the mean. This essentially
  increases the brightness.
- `-w 100`: window size in frames for the moving average. Larger window sizes
  reduce the noise but cause more motion blur. Testing suggests that beyond
  around 100 frames (for 30fps video) there is no noticeable improvement
  anymore.

After converting the video to a series of noise reduced images, you can use
ffmpeg to get a video again:

```sh
ffmpeg -framerate 30 -i output-%04d.jpg -r 30 -vf scale=3840:2160 -c:v libx264 -preset slow -crf 18 -pix_fmt yuv420p -movflags +faststart output.mp4
```

If you want to produce something DaVinci Resolve can read (e.g. for additional
color grading), you might want to use ProRes instead of H.264:

```sh
fmpeg -framerate 30 -i output-%04d.jpg -r 30 -vf scale=3840:2160 -c:v prores_ks -profile:v 3 -vendor apl0 -pix_fmt yuv422p10le output.mov
```

Keep in mind that ffmpeg can only encode 10bit ProRes but the JPEG files only
have 8bit information per channel.


Demo
----

Source video with noisy frames like this:

![Noisy frame](https://raw.githubusercontent.com/hackyourlife/brawshot/master/doc/noisy.jpg)

Noise reduced frame after using brawshot:

![Noise reduced frame](https://raw.githubusercontent.com/hackyourlife/brawshot/master/doc/output.jpg)


PS
--

I *hate* C++ but all Blackmagic Design SDKs purely use C++ APIs.
