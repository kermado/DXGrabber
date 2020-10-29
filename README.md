# DXGrabber
A lightweight DirectX screen grabber library, using the Windows Desktop Duplication API, based on the [NVIDIA Video SDK samples](https://github.com/NVIDIA/video-sdk-samples).

## Usage

The following steps illustrate how to use the library:

1. Call the `create()` function, which returns a pointer to the created grabber instance.
2. Call either `grab()` or `save()` as many times as required.
3. Call `release()` to free resources.

The `grab()` returns a pointer to a byte array image. The format required can be specified through the aguments to the function. The two formats currently supported are 24-bit RGB and 96-bit Darknet. Further details are provided below.

The `save()` function saves the captured screenshot to a PPM file.

### RGB

When the RGB format is specified, the byte array returned is in row-major format, with each channel stored as one byte [R, G, B, ...].

### Darknet

When the Darknet format is specified, the byte array returned should be interpreted as an array of floats. Each channel is stored successively, with each entry stored as one float [R, R, R, ..., G, G, G, ..., B, B, B].