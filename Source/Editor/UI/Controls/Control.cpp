#include "Control.hpp"
#include "ToolTip.hpp"

#include <UI/Graphics/Renderer.hpp>
#include <UI/System/Application.hpp>
#include <Studio/Impl.hpp>

#include <Hatch/Strings.h>

#include <algorithm>
#include <cmath>

Control* Control::MouseCaptured = NULL;
Control* Control::FocusCaptured = NULL;

void ControlCollection::Add(Control* control) {
    control->Parent = Owner;
    control->OnParentChanged(NULL);
    Items.push_back(control);
}
void ControlCollection::Clear() {
    for (auto it = Items.begin(); it != Items.end(); it++) {
        (*it)->Parent = NULL;
    }
    Items.clear();
}
int  ControlCollection::IndexOf(Control* control) {
    int index = 0;
    for (auto it = Items.begin(); it != Items.end(); it++) {
        if (control == (*it))
            return index;
        index++;
    }
    return -1;
}
bool ControlCollection::Contains(Control* control) {
    return IndexOf(control) != -1;
}
Control* ControlCollection::Last() {
    if (Count() > 0) {
        return Items[Count() - 1];
    }
    return NULL;
}
void ControlCollection::RemoveAt(int index) {
    Control* control = Items[index];
    control->Parent = NULL;
    Items.erase(Items.begin() + index);
}
void ControlCollection::Remove(Control* control) {
    int index = IndexOf(control);
    if (index != -1) {
        RemoveAt(index);
    }
}
int  ControlCollection::Count() {
    return (int)Items.size();
}

void ControlCollection::Sort() {
    std::sort(Items.begin(), Items.end(), [](Control* a, Control* b) {
        return a->ZIndex > b->ZIndex;
    });
}

Control::Control() {
    // memset(&AllowDrop, 0, sizeof(Control));

    Parent = NULL;
    Controls.Owner = this;
    Enabled = true;

    Location = { 0, 0 };
    Size = { 100, 100 };

    Padding = 0;
    Margin = 1;

    Cursor = SDL_CreateSystemCursor(SDL_SystemCursor::SDL_SYSTEM_CURSOR_ARROW);
    Strings::Init(&ToolTipText, 1);
}
void Control::ShowToolTip() {
    if (ToolTipText.Length > 0)
        UI::System::Application::Show(new ToolTip(&ToolTipText));

    ToolTipTimerStart = 0;
}
void Control::SetToolTipText(const char* text) {
    Strings::FromCString(&ToolTipText, text, 0);
}
void Control::Render() {
    auto screenRect = GetScreenRect();

    if (BackColor.A) {
        UI::Graphics::Renderer::DrawRect(&screenRect, BackColor);
    }

    SDL_Rect buffer;
    ClipStart(&buffer, &screenRect);

    if (DoZSorting)
        Controls.Sort();

    for (int i = 0, iSz = Controls.Count(); i < iSz; i++) {
        auto Child = Controls.Items[i];
        Child->Render();
    }

    ClipEnd(&buffer);
}

void Control::ClipStart(SDL_Rect* buffer, SDL_Rect* clip) {
    if (!buffer || !clip)
        return;

    SDL_Rect clipAdj = *clip;
    UI::Graphics::Renderer::DstRectAdjustment(&clipAdj);

    SDL_Rect newClip = clipAdj;
    SDL_RenderGetClipRect(UI::Graphics::Renderer::Renderer, buffer);

    if (!SDL_RectEmpty(buffer))
        SDL_IntersectRect(buffer, &clipAdj, &newClip);

    SDL_RenderSetClipRect(UI::Graphics::Renderer::Renderer, &newClip);
}
void Control::ClipEnd(SDL_Rect* buffer) {
    if (!buffer)
        return;

    if (!SDL_RectEmpty(buffer))
        SDL_RenderSetClipRect(UI::Graphics::Renderer::Renderer, buffer);
    else
        SDL_RenderSetClipRect(UI::Graphics::Renderer::Renderer, NULL);
}

int SAMPLE_SIZE = 2;

inline void setpixel(Color* image, int x, int y, int pitch, Color color) {
    if (color.A == 0) return;

	image[x + y * pitch] = color;
}
inline void setpixel4(Color* image, int pitch, int centerX, int centerY, int deltaX, int deltaY, Color $color) {
	// if ($color.A == 0) return;

	setpixel(image, centerX + deltaX, centerY + deltaY, pitch, $color);
	setpixel(image, centerX - deltaX - 1, centerY + deltaY, pitch, $color);
	setpixel(image, centerX + deltaX, centerY - deltaY - 1, pitch, $color);
	setpixel(image, centerX - deltaX - 1, centerY - deltaY - 1, pitch, $color);
}
inline float ipart(float x) {
    return std::floor(x);
}
inline float fpart(float x) {
    return x - std::floor(x);
}
inline float rfpart(float x) {
    return 1.0f - fpart(x);
}
bool pointInTriangle(int x1, int y1, int x2, int y2, int x3, int y3, int x, int y) {
    const int scale = 0x10000;
    int denominator = ((y2 - y3) * (x1 - x3) + (x3 - x2) * (y1 - y3));
    int a = ((y2 - y3) * (x - x3) + (x3 - x2) * (y - y3)) * scale / denominator;
    int b = ((y3 - y1) * (x - x3) + (x1 - x3) * (y - y3)) * scale / denominator;
    int c = scale - a - b;

    return 0 <= a && a <= scale && 0 <= b && b <= scale && 0 <= c && c <= scale;
}
bool pointInTriangleF(float x1, float y1, float x2, float y2, float x3, float y3, float x, float y) {
    float denominator = ((y2 - y3) * (x1 - x3) + (x3 - x2) * (y1 - y3));
    float a = ((y2 - y3) * (x - x3) + (x3 - x2) * (y - y3)) / denominator;
    float b = ((y3 - y1) * (x - x3) + (x1 - x3) * (y - y3)) / denominator;
    float c = 1.0f - a - b;

    return 0.0f <= a && a <= 1.0f && 0.0f <= b && b <= 1.0f && 0.0f <= c && c <= 1.0f;
}
void writeLine(Color* image, int pitch, int x0, int y0, int x1, int y1) {
// #define M_SWAP(a, b) { temp = a; a = b; b = temp; }

    const int maxTransparency = 0xFF; // 127

    float x0f = x0, y0f = y0, x1f = x1, y1f = y1;

    bool steep = M_ABS(y1 - y0) > M_ABS(x1 - x0);
    if (steep) {
        float temp;
        M_SWAP(x0, y0);
        M_SWAP(x1, y1);
    }
    if (x0 > x1) {
        float temp;
        M_SWAP(x0, x1);
        M_SWAP(y0, y1);
    }

    float dx = x1 - x0;
    float dy = y1 - y0;
    float gradient = dy / dx;
    if (dx == 0.0f)
        gradient = 1.0f;

    // handle first endpoint
    float xend = round(x0);
    float yend = y0 + gradient * (xend - x0);
    float xgap = rfpart(x0 + 0.5f);
    float xpxl1 = xend; // this will be used in the main loop
    float ypxl1 = ipart(yend);
    if (steep) {
        setpixel(image, ypxl1, xpxl1, pitch, Color(0xFFFFFF, maxTransparency * (rfpart(yend) * xgap)));
        setpixel(image, ypxl1 + 1.0f, xpxl1, pitch, Color(0xFFFFFF, maxTransparency * (fpart(yend) * xgap)));
    }
    else {
        setpixel(image, xpxl1, ypxl1, pitch, Color(0xFFFFFF, maxTransparency * (rfpart(yend) * xgap)));
        setpixel(image, xpxl1, ypxl1 + 1.0f, pitch, Color(0xFFFFFF, maxTransparency * (fpart(yend) * xgap)));
    }

    float intery = yend + gradient; // first y-intersection for the main loop

    // handle second endpoint
    xend = round(x1);
    yend = y1 + gradient * (xend - x1);
    xgap = fpart(x1 + 0.5f);
    float xpxl2 = xend; //this will be used in the main loop
    float ypxl2 = ipart(yend);
    if (steep) {
        setpixel(image, ypxl2, xpxl2, pitch, Color(0xFFFFFF, maxTransparency * (rfpart(yend) * xgap)));
        setpixel(image, ypxl2 + 1.0f, xpxl2, pitch, Color(0xFFFFFF, maxTransparency * (fpart(yend) * xgap)));
    }
    else {
        setpixel(image, xpxl2, ypxl2, pitch, Color(0xFFFFFF, maxTransparency * (rfpart(yend) * xgap)));
        setpixel(image, xpxl2, ypxl2 + 1.0f, pitch, Color(0xFFFFFF, maxTransparency * (fpart(yend) * xgap)));
    }

    // main loop
    if (steep) {
        for (float x = xpxl1 + 1.0f; x <= xpxl2 - 1.0f; x += 1.0f) {
            setpixel(image, ipart(intery), x, pitch, Color(0xFFFFFF, maxTransparency * (rfpart(intery))));
            setpixel(image, ipart(intery) + 1.0f, x, pitch, Color(0xFFFFFF, maxTransparency * (fpart(intery))));
            intery += gradient;
        }
    }
    else {
        for (float x = xpxl1 + 1.0f; x <= xpxl2 - 1.0f; x += 1.0f) {
            setpixel(image, x, ipart(intery), pitch, Color(0xFFFFFF, maxTransparency * (rfpart(intery))));
            setpixel(image, x, ipart(intery) + 1.0f, pitch, Color(0xFFFFFF, maxTransparency * (fpart(intery))));
            intery += gradient;
        }
    }

    // // upper and lower halves
    // int quarter = (int)round(radiusX2 / sqrt(radiusX2 + radiusY2));
    // for (int x = 0; x <= quarter; x++) {
    // 	float y = radiusY * sqrt(1.0f - x * x / radiusX2);
    // 	float $error = y - floor(y);
    // 	float $transparency = round($error * maxTransparency);
    // 	setpixel4(image, pitch, centerX, centerY, x, floor(y), Color(0xFFFFFF, $transparency));
    // 	setpixel4(image, pitch, centerX, centerY, x, floor(y) - 1, Color(0xFFFFFF, maxTransparency - $transparency));
    // }

#undef M_SWAP
}
void writeCircleStroke(Color* image, int pitch, int radiusX, int radiusY, int centerX, int centerY) {
	float radiusX2 = radiusX * radiusX;
	float radiusY2 = radiusY * radiusY;
	int maxTransparency = 0xFF; // 127
	// upper and lower halves
	int quarter = (int)round(radiusX2 / sqrt(radiusX2 + radiusY2));
	for (int x = 0; x <= quarter; x++) {
		float y = radiusY * sqrt(1.0f - x * x / radiusX2);
		float $error = y - floor(y);
		float $transparency = round($error * maxTransparency);
		setpixel4(image, pitch, centerX, centerY, x, floor(y), Color(0xFFFFFF, $transparency));
		setpixel4(image, pitch, centerX, centerY, x, floor(y) - 1, Color(0xFFFFFF, maxTransparency - $transparency));
	}
	// right and left halves
	quarter = (int)round(radiusY2 / sqrt(radiusX2 + radiusY2));
	for (int y = 0; y <= quarter; y++) {
		float x = radiusX * sqrt(1.0f - y * y / radiusY2);
		float $error = x - floor(x);
		float $transparency = round($error * maxTransparency);
		setpixel4(image, pitch, centerX, centerY, floor(x), y, Color(0xFFFFFF, $transparency));
		setpixel4(image, pitch, centerX, centerY, floor(x) - 1, y, Color(0xFFFFFF, maxTransparency - $transparency));
	}
}
void writeCircleFilled(Color* image, int pitch, int radiusX, int radiusY, int centerX, int centerY, int offsetX = 0, int offsetY = 0) {
	for (float y = 0; y < radiusY * 2.0f; y++) {
		for (float x = 0; x < radiusX * 2.0f; x++) {
			float deltaX = radiusX - x - 0.5f;
			float deltaY = radiusY - y - 0.5f;
			float distance = sqrt(deltaX * deltaX + deltaY * deltaY);
			int alpha = M_CLAMP(radiusX - distance, 0.0f, 1.0f) * 0xFF;
			setpixel(image, x + offsetX, y + offsetY, pitch, Color(0xFFFFFF, alpha));
		}
	}
}
void writeTriangleStroke(Color* image, int pitch, int x0, int y0, int x1, int y1, int x2, int y2) {
    writeLine(image, pitch, x0, y0, x1, y1);
    writeLine(image, pitch, x1, y1, x2, y2);
    writeLine(image, pitch, x2, y2, x0, y0);
}
void writeTriangleFilled(Color* image, int pitch, int height, int x0, int y0, int x1, int y1, int x2, int y2) {
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < pitch; x++) {
            int alpha = 0; // Multi-sampling
            alpha += pointInTriangleF(x0, y0, x1, y1, x2, y2, x + 0.25f, y + 0.25f) * 0x40;
            alpha += pointInTriangleF(x0, y0, x1, y1, x2, y2, x + 0.75f, y + 0.25f) * 0x40;
            alpha += pointInTriangleF(x0, y0, x1, y1, x2, y2, x + 0.25f, y + 0.75f) * 0x40;
            alpha += pointInTriangleF(x0, y0, x1, y1, x2, y2, x + 0.75f, y + 0.75f) * 0x3F;
            setpixel(image, x, y, pitch, Color(0xFFFFFF, alpha));
        }
    }
}

union vec2 {
    struct {
        float x;
        float y;
    };
    struct {
        float a;
        float b;
    };
    float v[2];
};
float step(float edge, float x) {
    if (x < edge)
        return 0.0f;
    return 1.0f;
}
vec2 step(vec2 edge, vec2 x) {
    if (x.x < edge.x) {
        if (x.y < edge.y)
            return { 0.0f, 0.0f };
        else
            return { 0.0f, 1.0f };
    }
    if (x.y < edge.y)
        return { 1.0f, 0.0f };
    return { 1.0f, 1.0f };
}
float mix(float x, float y, float a) {
    if (a <= 0.0f) return x;
    if (a >= 1.0f) return y;
    return x + a * (y - x);
}
vec2 max2(vec2 a, vec2 b) {
    if (a.x > b.x) {
        if (a.y > b.y)
            return { a.x, a.y };
        else
            return { a.x, b.y };
    }
    if (a.y < b.y)
        return { b.x, a.y };
    return { b.x, b.y };
}
vec2 abs2(vec2 n) {
    if (n.x < 0.0f) {
        if (n.y < 0.0f)
            return { -n.x, -n.y };
        else
            return { -n.x, n.y };
    }
    if (n.y < 0.0f)
        return { n.x, -n.y };
    return { n.x, n.y };
}
vec2 add(vec2 a, vec2 b) {
    return { a.x + b.x, a.y + b.y };
}
vec2 sub(vec2 a, vec2 b) {
    return { a.x - b.x, a.y - b.y };
}
float length(vec2 n) {
    return sqrt(n.x * n.x + n.y * n.y);
}
int pointInRoundRect(vec2 pos, vec2 extents, float cornerRadii[4]) {
    vec2 s = step(pos, { 0.0f, 0.0f });
    float r = mix(
        mix(cornerRadii[1], cornerRadii[2], s.y),
        mix(cornerRadii[0], cornerRadii[3], s.y),
        s.x);
    vec2 c = max2(sub(add(abs2(pos), { r, r }), extents), { 0.0f, 0.0f });
    float len = c.x * c.x + c.y * c.y - r * r;
    return len < 0.0f;
    // return abs(sqrt(len)) - 4.0f < 0.0f;
}
void writeRoundedRectangleFilled(Color* image, int pitch, int rowCount, float x, float y, float width, float height, float radTL, float radTR, float radBL, float radBR) {
    vec2 extents = { width / 2.0f, height / 2.0f };
    float corners[4] = { radTL, radTR, radBR, radBL };
    for (int row = 0; row < rowCount; row++) {
        for (int col = 0; col < pitch; col++) {
            int alpha = 0; // Multi-sampling
            vec2 pos = { col - (x + extents.x), row - (y + extents.y) };
            switch (SAMPLE_SIZE) {
                case 1:
                    alpha = pointInRoundRect({ pos.x + 0.25f, pos.y + 0.25f }, extents, corners) * 0xFF;
                    break;
                case 2:
                    alpha = pointInRoundRect({ pos.x + 0.25f, pos.y + 0.25f }, extents, corners) * 0x40
                        + pointInRoundRect({ pos.x + 0.75f, pos.y + 0.25f }, extents, corners) * 0x40
                        + pointInRoundRect({ pos.x + 0.25f, pos.y + 0.75f }, extents, corners) * 0x40
                        + pointInRoundRect({ pos.x + 0.75f, pos.y + 0.75f }, extents, corners) * 0x3F;
                    break;
            }
            if (alpha)
                setpixel(image, col, row, pitch, Color(0xFFFFFF, alpha));
        }
    }
}

void CreateShapeTexture_EllipseStroke(SDL_Texture** texture, int width, int height) {
	Color* image = (Color*)calloc(width * height, sizeof(Color));
	if (image == NULL) {
        *texture = NULL;
		return;
    }

	writeCircleStroke(image, width, width / 2, height / 2, width / 2, height / 2);
	if (!Studio::Textures::CreateTextureFromSTBI(texture, (unsigned char*)image, width, height)) {
        *texture = NULL;
        free(image);
		return;
    }

	free(image);
}
void CreateShapeTexture_EllipseFill(SDL_Texture** texture, int width, int height) {
	Color* image = (Color*)calloc(width * height, sizeof(Color));
    if (image == NULL) {
        *texture = NULL;
		return;
    }

	writeCircleFilled(image, width, width / 2, height / 2, width / 2, height / 2);
    if (!Studio::Textures::CreateTextureFromSTBI(texture, (unsigned char*)image, width, height)) {
        *texture = NULL;
        free(image);
		return;
    }

	free(image);
}
void CreateShapeTexture_Radio(SDL_Texture** texture, int width, int height) {
    Color* image = (Color*)calloc(width * height, sizeof(Color));
    if (image == NULL) {
        *texture = NULL;
        return;
    }

    const int adjustment = 3;

    writeCircleStroke(image, width, width / 2, height / 2, width / 2, height / 2);
    writeCircleFilled(image,
        width,
        (width / 2) - adjustment,
        (height / 2) - adjustment,
        (width / 2) - adjustment,
        (height / 2) - adjustment,
        adjustment,
        adjustment
    );

    if (!Studio::Textures::CreateTextureFromSTBI(texture, (unsigned char*)image, width, height)) {
        *texture = NULL;
        free(image);
        return;
    }

    free(image);
}

void CreateShapeTexture_RoundRectFill(SDL_Texture** texture, int width, int height, int c0, int c1, int c2, int c3) {
    Color* image = (Color*)calloc(width * height, sizeof(Color));
    if (image == NULL) {
        *texture = NULL;
        return;
    }

    writeRoundedRectangleFilled(image, width, height, 0.0f, 0.0f, width, height, c0, c1, c2, c3);
    if (!Studio::Textures::CreateTextureFromSTBI(texture, (unsigned char*)image, width, height)) {
        *texture = NULL;
        free(image);
        return;
    }

    free(image);
}

void CreateShapeTexture_TriangleStroke(SDL_Texture** texture, int width, int height) {
    Color* image = (Color*)calloc(width * height, sizeof(Color));
	if (image == NULL) {
        *texture = NULL;
		return;
    }

	writeLine(image, width, 0, 0,          width - 1, height / 2);
    writeLine(image, width, 0, height - 1, width - 1, height / 2);

    writeLine(image, width, 0, 0, 0, height - 1);

	if (!Studio::Textures::CreateTextureFromSTBI(texture, (unsigned char*)image, width, height)) {
        *texture = NULL;
        free(image);
		return;
    }

	free(image);
}
void CreateShapeTexture_TriangleFill(SDL_Texture** texture, int width, int height) {
    int pitch = width;
    Color* image = (Color*)calloc(width * height, sizeof(Color));
    if (image == NULL) {
        *texture = NULL;
        return;
    }

    writeTriangleFilled(image, pitch, height,
        0, 0,
        width - 1, height / 2,
        0, height - 1);

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "2");
    if (!Studio::Textures::CreateTextureFromSTBI(texture, (unsigned char*)image, width, height)) {
        *texture = NULL;

        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
        free(image);
        return;
    }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    free(image);
}
