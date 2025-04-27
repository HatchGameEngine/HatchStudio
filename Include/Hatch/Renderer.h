#pragma once

namespace Renderer {
    extern int RendererW, RendererH;

    bool Init();
    void Clear();
    void TransferFrameBuffers();
    void Present();
    void Dispose();

    void UpdateViewOutputs();

    void UpdateTexture420(int width, int height, Uint8* pixelY, Uint8* pixelU, Uint8* pixelV, int strideY, int strideU, int strideV);
    void UpdateTexture422(int width, int height, Uint8* pixelY, Uint8* pixelU, Uint8* pixelV, int strideY, int strideU, int strideV);
    void UpdateTexture444(int width, int height, Uint8* pixelY, Uint8* pixelU, Uint8* pixelV, int strideY, int strideU, int strideV);
}
