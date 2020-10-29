#pragma once

// Simple Desktop Duplication API for screen capture.
// Based on NVIDIA's Video SDK Samples: https://github.com/NVIDIA/video-sdk-samples

#define EXPORT extern "C" __declspec(dllexport)

class grabber;

// Creates a grabber instance.
EXPORT grabber* create();

// Releases the specified grabber instance.
EXPORT void release(grabber* grabber);

// Grabs the specified region of the screen.
//
// Arguments:
// - x = The left side of the region (in screen coordinates).
// - y = The top side of the region (in screen coordinates).
// - width = The width of the region.
// - height = The height of the region.
// - format = 0 for RGB (24-bit), 1 for Darknet 96-bit.
// - timeouit = The maximum time to wait when capturing a frame.
// - wait = Whether to (blocking) wait for a frame to become available.
// - frames = Set to the number of frames that were accumulated (out param).
//
// Returns: Pointer to the image. Do not delete this!
EXPORT BYTE* grab(grabber* grabber, int x, int y, int width, int height, int format, int timeout, bool wait, int* frames);

// Saves the specified region of the screen to a PPM file.
//
// Arguments:
// - x = The left side of the region (in screen coordinates).
// - y = The top side of the region (in screen coordinates).
// - width = The width of the region.
// - height = The height of the region.
// - format = 0 for RGB (24-bit), 1 for Darknet 96-bit.
// - timeouit = The maximum time to wait when capturing a frame.
// - filepath = The path to the file, which is to be saved.
//
// Returns: True if successful, false otherwise.
EXPORT bool save(grabber* grabber, int x, int y, int width, int height, int format, int timeout, const char* filepath);