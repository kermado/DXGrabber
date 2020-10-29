/*
 * Copyright (c) 2019, NVIDIA CORPORATION. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of NVIDIA CORPORATION nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "defs.h"
#include "dda_impl.h"
#include "grabber.h"

class grabber
{
private:
    DDAImpl* dda_wrapper = nullptr;
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* device_context = nullptr;
    ID3D11Texture2D* duplication_texture = nullptr;
    ID3D11Texture2D* output_texture = nullptr;
    BYTE* output_image = nullptr;
    INT output_image_length;
    UINT duplication_width = 0;
    UINT duplication_height = 0;

public:
    BYTE* get_output_image() { return output_image; }

private:
    // Initialize DXGI pipeline
    HRESULT init_dxgi()
    {
        HRESULT hr = S_OK;

        /// Driver types supported
        D3D_DRIVER_TYPE driver_types[] =
        {
            D3D_DRIVER_TYPE_HARDWARE,
            D3D_DRIVER_TYPE_WARP,
            D3D_DRIVER_TYPE_REFERENCE,
        };

        UINT num_driver_types = ARRAYSIZE(driver_types);

        // Feature levels supported
        D3D_FEATURE_LEVEL feature_levels[] =
        {
            D3D_FEATURE_LEVEL_11_0,
            D3D_FEATURE_LEVEL_10_1,
            D3D_FEATURE_LEVEL_10_0,
            D3D_FEATURE_LEVEL_9_1
        };

        UINT num_feature_levels = ARRAYSIZE(feature_levels);
        D3D_FEATURE_LEVEL feature_level = D3D_FEATURE_LEVEL_11_0;

        // Create device
        for (UINT driver_type_index = 0; driver_type_index < num_driver_types; ++driver_type_index)
        {
            hr = D3D11CreateDevice(nullptr, driver_types[driver_type_index], nullptr, 0, feature_levels, num_feature_levels, D3D11_SDK_VERSION, &device, &feature_level, &device_context);
            if (SUCCEEDED(hr))
            {
                // Device creation succeeded, no need to loop anymore.
                break;
            }
        }

        return hr;
    }

    HRESULT init_duplication()
    {
        HRESULT hr = S_OK;
        if (!dda_wrapper)
        {
            dda_wrapper = new DDAImpl(device, device_context);
            hr = dda_wrapper->Init();
            returnIfError(hr);
        }

        return hr;
    }

    HRESULT init_output_texture()
    {
        HRESULT hr = S_OK;

        if (output_texture == nullptr)
        {
            D3D11_TEXTURE2D_DESC duplication_desc;
            duplication_texture->GetDesc(&duplication_desc);

            duplication_width = duplication_desc.Width;
            duplication_height = duplication_desc.Height;

            D3D11_TEXTURE2D_DESC desc;
            desc.Width = duplication_width;
            desc.Height = duplication_height;
            desc.Format = duplication_desc.Format;
            desc.ArraySize = 1;
            desc.BindFlags = 0;
            desc.MiscFlags = 0;
            desc.SampleDesc.Count = 1;
            desc.SampleDesc.Quality = 0;
            desc.MipLevels = 1;
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            desc.Usage = D3D11_USAGE_STAGING;

            hr = device->CreateTexture2D(&desc, NULL, &output_texture);
        }

        return hr;
    }

    void init_output_image(int width, int height, int bytes_per_pixel)
    {
        INT new_output_image_length = width * height * bytes_per_pixel;
        if (output_image != nullptr && output_image_length != new_output_image_length)
        {
            delete[] output_image;
            output_image = nullptr;
        }

        if (output_image == nullptr)
        {
            output_image_length = new_output_image_length;
            output_image = new BYTE[output_image_length];
            ZeroMemory(output_image, output_image_length);
        }
    }

public:
    /// Initialize demo application
    HRESULT init()
    {
        HRESULT hr = S_OK;

        hr = init_dxgi();
        returnIfError(hr);

        hr = init_duplication();
        returnIfError(hr);

        return hr;
    }

    /// Capture a frame using DDA
    HRESULT capture(int wait, int& accumulated_frames)
    {
        HRESULT hr = dda_wrapper->GetCapturedFrame(&duplication_texture, wait, accumulated_frames);
        return hr;
    }

    HRESULT save(INT output_image_x, INT output_image_y, INT output_image_width, INT output_image_height, int format)
    {
        HRESULT hr = S_OK;

        hr = init_output_texture();
        returnIfError(hr);

        device_context->CopyResource(output_texture, duplication_texture);
        SAFE_RELEASE(duplication_texture);

        IDXGISurface1* surface;
        hr = output_texture->QueryInterface(IID_PPV_ARGS(&surface));
        returnIfError(hr);

        DXGI_MAPPED_RECT rect;
        hr = surface->Map(&rect, DXGI_MAP_READ);
        returnIfError(hr);

        BYTE* output_texture_data = rect.pBits;

        hr = surface->Unmap();
        returnIfError(hr);

        INT out_index = 0;

        // The texture is stored [B, G, R, A, ...], with each entry 1 byte.
        // The RGB format is [R, G, B, ...], with each entry 1 byte.
        // The Darknet format is [R, R, R, ..., G, G, G, ..., B, B, B], with each entry one float.
        switch (format)
        {
        case 0: // RGB
            init_output_image(output_image_width, output_image_height, 3);
            for (INT y = 0; y < output_image_height; ++y)
            {
                INT in_row_start = (output_image_y + y) * duplication_width + output_image_x;
                for (INT x = 0; x < output_image_width; ++x)
                {
                    INT inPixelStart = in_row_start + x;
                    output_image[out_index++] = output_texture_data[inPixelStart * 4 + 2];
                    output_image[out_index++] = output_texture_data[inPixelStart * 4 + 1];
                    output_image[out_index++] = output_texture_data[inPixelStart * 4];
                }
            }
            break;
        case 1: // Darknet
            init_output_image(output_image_width, output_image_height, 12);
            {
                float* darknet_output_image = (float*)output_image;
                for (INT c = 0; c < 3; ++c)
                {
                    for (INT y = 0; y < output_image_height; ++y)
                    {
                        INT in_row_start = (output_image_y + y) * duplication_width + output_image_x;
                        for (INT x = 0; x < output_image_width; ++x)
                        {
                            INT in_index = (in_row_start + x) * 4 + (2 - c);
                            darknet_output_image[out_index++] = (float)output_texture_data[in_index] / 255.0F;
                        }
                    }
                }
            }
            break;
        default: // Unknown format.
            delete[] output_image;
            output_image = nullptr;
            output_image_length = 0;
        }

        return hr;
    }

    void cleanup(bool end = true)
    {
        if (dda_wrapper)
        {
            dda_wrapper->Cleanup();

            delete dda_wrapper;
            dda_wrapper = nullptr;
        }

        SAFE_RELEASE(duplication_texture);
        
        if (end)
        {
            SAFE_RELEASE(output_texture);
            SAFE_RELEASE(device);
            SAFE_RELEASE(device_context);

            if (output_image != nullptr)
            {
                delete[] output_image;
                output_image = nullptr;
            }
        }
    }

    grabber() {}

    ~grabber()
    {
        cleanup(true);
    }
};

grabber* create()
{
    HRESULT hr = S_OK;

    grabber* instance = new grabber();
    hr = instance->init();

    if (hr != S_OK)
    {
        delete instance;
        return nullptr;
    }

    return instance;
}

void release(grabber* grabber)
{
    if (grabber != nullptr)
    {
        delete grabber;
    }
}

BYTE* grab(grabber* grabber, int x, int y, int width, int height, int format, int timeout, bool wait, int* frames)
{
    *frames = 0;
    if (grabber == nullptr || width <= 0 || height <= 0) { return nullptr; }

    HRESULT hr = S_OK;
    int accumulated_frames = 0;

    do
    {
        // Try to capture a frame.
        hr = grabber->capture(timeout, accumulated_frames);
        if (hr == DXGI_ERROR_WAIT_TIMEOUT)
        {
            return nullptr;
        }

        if (FAILED(hr))
        {
            // Re-try with a new DDA.
            grabber->cleanup();
            hr = grabber->init();
            if (FAILED(hr))
            {
                // Failed to initialize DDA.
                return nullptr;
            }

            // Try again to capture a frame.
            hr = grabber->capture(timeout, accumulated_frames);
        }

        if (FAILED(hr))
        {
            return nullptr;
        }
    }
    while (wait && accumulated_frames <= 0);

    hr = grabber->save(x, y, width, height, format);
    if (FAILED(hr))
    {
        return nullptr;
    }

    *frames = accumulated_frames;
    return grabber->get_output_image();
}

bool save(grabber* grabber, int x, int y, int width, int height, int format, int timeout, const char* filepath)
{
    int frames = 0;
    BYTE* data = grab(grabber, x, y, width, height, format, timeout, true, &frames);
    if (data != nullptr && format == 0) // Only supports RGB.
    {
        FILE* fp = fopen(filepath, "wb");
        fprintf(fp, "P6\n%d %d\n255\n", width, height);

        for (INT row = 0; row < height; ++row)
        {
            for (INT col = 0; col < width; ++col)
            {
                INT start = 3 * (row * width + col);
                fwrite(&data[start], 1, 3, fp);
            }
        }

        fclose(fp);
        return true;
    }

    return false;
}