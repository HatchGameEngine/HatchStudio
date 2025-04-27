// Hatch Required includes
#include PLATFORM_SETTINGS
#include <Hatch/Primitives.h>
#include <Hatch/Types.h>

#include <Hatch/Renderer.h>

// Hatch Runtime includes
#include <Hatch/Diagnostics.h>
#include <Hatch/Game.h>
#include <Hatch/Graphics.h>
#include <Hatch/Settings.h>

namespace Renderer {
    int RendererW, RendererH;

    void UpdateViewOutputs() {
        int temp;

        ViewOutput* viewOutput = &Graphics::ViewOutputs[0];
        if (viewOutput->Active) {
            View* view = &Graphics::Views[viewOutput->ViewIndex];

            // Determine output size
            switch (viewOutput->ScaleType) {
                case VOSCALE_NONE:
                    viewOutput->Width = view->Width;
                    viewOutput->Height = view->Height;
                    break;
                case VOSCALE_FIT_TO_SCREEN:
                    if ((RendererW << 16) / view->Width > (RendererH << 16) / view->Height) {
                        viewOutput->Width = view->Width * RendererH / view->Height;
                        viewOutput->Height = RendererH;
                    }
                    else {
                        viewOutput->Width = RendererW;
                        viewOutput->Height = view->Height * RendererW / view->Width;
                    }
                    break;
                case VOSCALE_COVER_TO_SCREEN:
                    if ((RendererW << 16) / view->Width > (RendererH << 16) / view->Height) {
                        viewOutput->Width = RendererW;
                        viewOutput->Height = view->Height * RendererW / view->Width;
                    }
                    else {
                        viewOutput->Width = view->Width * RendererH / view->Height;
                        viewOutput->Height = RendererH;
                    }
                    break;
                case VOSCALE_STRETCH_TO_SCREEN:
                    viewOutput->Width = RendererW;
                    viewOutput->Height = RendererH;
                    break;
                case VOSCALE_RESIZE_TO_SCREEN:
                    viewOutput->Width = RendererW;
                    viewOutput->Height = RendererH;

                    // Update dirty sizes
                    if (view->DirtySize) {
                        if ((RendererW << 16) / view->Width > (RendererH << 16) / view->Height)
                            Graphics::View_SetSize(viewOutput->ViewIndex, view->Height * RendererW / RendererH, view->Height);
                        else if ((RendererW << 16) / view->Width < (RendererH << 16) / view->Height)
                            Graphics::View_SetSize(viewOutput->ViewIndex, view->Width, view->Width * RendererH / RendererW);

                        view->DirtySize = false;
                    }
                    break;
            }

            // Determine output position
            viewOutput->X = (RendererW - viewOutput->Width) / 2;
            viewOutput->Y = (RendererH - viewOutput->Height) / 2;

            // Clear framebuffer pixels
            // memset(view->Pixels, 0, sizeof(Pixel) * view->Width * view->Height);
        }
    }
}
