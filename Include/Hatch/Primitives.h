#pragma once

#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <limits.h>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#if defined(_IPHONE) || defined(_MACOS)
#define offsetof __offsetof
#endif

#define ZERO_OUT(object) memset(&object, 0, sizeof(object))
// #define ZERO_OUT_ARRAY(object) memset(object, 0, sizeof(object))

typedef unsigned char Uint8;
typedef unsigned short Uint16;
typedef unsigned int Uint32;
typedef uint64_t Uint64;
typedef signed char Sint8;
typedef signed short Sint16;
typedef signed int Sint32;
typedef int64_t Sint64;
// typedef Sint32 bool; // To ensure uniform boolean sizes across platforms.
typedef signed int Resource;
typedef signed int ClassID;
typedef const char* CString;
// typedef Uint16 Tile;

union  Tile {
    struct { Uint16 ID:12; Uint16 FlipX:1; Uint16 FlipY:1; Uint16 PlaneA:2; Uint16 PlaneB:2; };
    Uint32 Full;

    Tile() {
        Full = 0;
    }
    Tile(Uint32 tile) {
        Full = tile;
    }
    operator Uint32() const { return Full; }
};

union  Subpixels {
    struct { Uint16 Fract; Sint16 Whole; };
    Sint32 Full;

    Subpixels() {
        Full = 0;
    }
    Subpixels(const int& a) {
        Full = a;
    }
    Subpixels(Sint16 whole, Uint16 fract) {
        Whole = whole;
        Fract = fract;
    }

    operator int() const { return Full; }

    Subpixels operator-() {
        return Subpixels(-Full);
    }

    Subpixels operator=(const int& b) {
        Full = b;
        return *this;
    }
    Subpixels operator+=(const int& b) {
        Full += b;
        return *this;
    }
    Subpixels operator-=(const int& b) {
        Full -= b;
        return *this;
    }
    Subpixels operator*=(const int& b) {
        Full *= b;
        return *this;
    }
    Subpixels operator/=(const int& b) {
        Full /= b;
        return *this;
    }
};
struct Vector2 {
    Subpixels X;
    Subpixels Y;

    Vector2(int x = 0, int y = 0) {
        X.Full = x;
        Y.Full = y;
    }
    Vector2(Subpixels x, Subpixels y) {
        X = x;
        Y = y;
    }

    #define CUSTOM_OPERATOR(op) \
    Vector2 operator op(const Vector2& b) const { \
        return Vector2(X op b.X, Y op b.Y); \
    } \
    Vector2& operator op##=(const Vector2& b) { \
        X = X op b.X;\
        Y = Y op b.Y;\
        return *this; \
    }

    CUSTOM_OPERATOR(+);
    CUSTOM_OPERATOR(-);
    CUSTOM_OPERATOR(*);
    CUSTOM_OPERATOR(/);

    Vector2 operator-() const {
        return Vector2(-X, -Y);
    }

    #undef CUSTOM_OPERATOR
};
struct Vector3 {
    Subpixels X;
    Subpixels Y;
    Subpixels Z;

    Vector3(int x = 0, int y = 0, int z = 0) {
        X.Full = x;
        Y.Full = y;
        Z.Full = z;
    }
    Vector3(Subpixels x, Subpixels y, Subpixels z) {
        X = x;
        Y = y;
        Z = z;
    }

    Vector3 operator+(const Vector3& b) {
        return Vector3(X + b.X, Y + b.Y, Z + b.Z);
    }
    Vector3 operator-(const Vector3& b) {
        return Vector3(X - b.X, Y - b.Y, Z - b.Z);
    }
    Vector3 operator*(const Vector3& b) {
        return Vector3(X * b.X, Y * b.Y, Z * b.Z);
    }
    Vector3 operator/(const Vector3& b) {
        return Vector3(X / b.X, Y / b.Y, Z / b.Z);
    }
};
struct Hitbox {
    int Left;
    int Top;
    int Right;
    int Bottom;
    Hitbox() { Top = Left = Right = Bottom = 0; }
    Hitbox(int left, int right, int top, int bottom) {
        Top = top;
        Left = left;
        Right = right;
        Bottom = bottom;
    }
};

struct String {
    Sint16* Text;
    Uint16  Length;
    Uint16  Capacity;
    Uint8   Encoding;
};

enum StringEncoding : Uint8 {
    UTF8,
    UTF16,
};

union  Color {
    #if defined TARGET_ENDIAN_BE
        #if defined TARGET_COLORFMT_RGBA
            struct { Uint8 A; Uint8 B; Uint8 G; Uint8 R; };
        #elif defined TARGET_COLORFMT_ARGB
            struct { Uint8 B; Uint8 G; Uint8 R; Uint8 A; };
        #else // TARGET_COLORFMT_BGRA
            struct { Uint8 A; Uint8 R; Uint8 G; Uint8 B; };
        #endif
    #else
        #if defined TARGET_COLORFMT_RGBA
            struct { Uint8 R; Uint8 G; Uint8 B; Uint8 A; };
        #elif defined TARGET_COLORFMT_ABGR
            struct { Uint8 A; Uint8 B; Uint8 G; Uint8 R; };
        #elif defined TARGET_COLORFMT_ARGB
            struct { Uint8 A; Uint8 R; Uint8 G; Uint8 B; };
        #else // TARGET_COLORFMT_BGRA
            struct { Uint8 B; Uint8 G; Uint8 R; Uint8 A; };
        #endif
    #endif
    Uint32 Full;

    Color() {
        Full = 0;
    }
    Color(Uint8 r, Uint8 g, Uint8 b, Uint8 a = 0xFFU) {
        R = r;
        G = g;
        B = b;
        A = a;
    }
    Color(Uint32 color, Uint8 a) {
        R = (color >> 16) & 0xFF;
        G = (color >> 8) & 0xFF;
        B = (color) & 0xFF;
        A = a;
    }
    Color(Uint32 color) {
        Full = color;
    }
    operator Uint32() const { return Full; }
};
union  Pixel {
    struct { Uint16 A:1; Uint16 B:5; Uint16 G:5; Uint16 R:5; };
    Uint16 Full;

    Pixel() {
        Full = 0;
        A = 1;
    }
    Pixel(Uint8 r, Uint8 g, Uint8 b) {
        R = r;
        G = g;
        B = b;
        A = 1;
    }
    Pixel(Color c) {
        R = c.R >> 3;
        G = c.G >> 3;
        B = c.B >> 3;
        A = 1;
        // printf("c %02X %02X %02X p %02X %02X %02X f %04X\n", c.R, c.G, c.B, R, G, B, Full);
    }
    Pixel(Uint16 color) {
        Full = color;
    }
    operator Uint16() const { return Full; }
};

struct Hash {
    Uint32 A;
    Uint32 B;
    Uint32 C;
    Uint32 D;
    bool operator==(const Hash& b) { return A == b.A && B == b.B && C == b.C && D == b.D; }
    bool operator!=(const Hash& b) { return A != b.A || B != b.B || C != b.C || D != b.D; }
};
