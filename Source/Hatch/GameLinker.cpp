#include PLATFORM_SETTINGS
#include <Hatch/Primitives.h>
#include <Hatch/Types.h>

#include <Hatch/GameLinker.h>

#include <Hatch/Hashing/MD5.h>

#include <Hatch/Audio.h>
#include <Hatch/Classes.h>
#include <Hatch/Collision.h>
#include <Hatch/Game.h>
#include <Hatch/Graphics.h>
#include <Hatch/Input.h>
#include <Hatch/Math.h>
#include <Hatch/Memory.h>
#include <Hatch/Resources.h>
#include <Hatch/Scene.h>
#include <Hatch/Services.h>
#include <Hatch/Strings.h>
#include <Hatch/Video.h>

#include <math.h>

bool Animator_Set(Animator* animator, Resource sprite, int animationIndex, int frameIndex, bool resetFrame) {
    if (!animator)
        return false;

    if (sprite < 0 || sprite >= MAX_SPRITES) {
        animator->StartFrame = NULL;
        return false;
    }

    Resources::ResSprite* resSprite = &Resources::ResourceSprites[sprite];
    if (animationIndex >= resSprite->SpriteData.AnimationCount)
        return false;

    auto animationPtr = &Resources::ResourceSprites[sprite].SpriteData.Animations[animationIndex];
    auto startFramePtr = &resSprite->SpriteData.Frames[resSprite->SpriteData.Animations[animationIndex].StartFrameIndex];
    if (animator->StartFrame == startFramePtr && !resetFrame)
        return false;

    animator->StartFrame = startFramePtr;
    animator->Time = 0;
    animator->FrameIndex = frameIndex;
    animator->FrameCount = animationPtr->FrameCount;
    animator->FrameDuration = startFramePtr[frameIndex].Duration;
    animator->Speed = animationPtr->Speed;
    animator->RotationFlag = animationPtr->RotationFlag;
    animator->FrameLoop = animationPtr->LoopFrameIndex;
    animator->PrevAnimationIndex = animator->AnimationIndex;
    animator->AnimationIndex = animationIndex;
    return true;
}
bool Animator_Set3D(Animator* animator, Resource mesh, int animationSpeed, int frameLoopIndex, int frameIndex, bool resetFrame) {
    if (!animator)
        return false;

    if (mesh < 0 || mesh >= MAX_MESHES) {
        animator->StartFrame = NULL;
        return false;
    }

    if (animator->AnimationIndex != mesh || resetFrame) {
        animator->StartFrame = (Frame*)1;
        animator->Time = 0;
        animator->FrameIndex = frameIndex;
        animator->FrameCount = Resources::ResourceMeshes[mesh].MeshData.FrameCount;
        animator->FrameDuration = 256;
        animator->Speed = animationSpeed;
        animator->FrameLoop = frameLoopIndex;
        animator->PrevAnimationIndex = animator->AnimationIndex;
        animator->AnimationIndex = mesh;

        return true;
    }

    return false;
}
void Animator_Update(Animator* animator) {
    if (animator) {
        if (animator->StartFrame) {
            animator->Time += animator->Speed;
            // Mesh Animation
            if (animator->StartFrame == (Frame*)1) {
                while (animator->Time > animator->FrameDuration) {
                    animator->FrameIndex++;
                    animator->Time -= animator->FrameDuration;
                    if (animator->FrameIndex >= animator->FrameCount)
                        animator->FrameIndex = animator->FrameLoop;
                }
            }
            // Sprite Animation
            else {
                while (animator->Time > animator->FrameDuration) {
                    animator->FrameIndex++;
                    animator->Time -= animator->FrameDuration;
                    if (animator->FrameIndex >= animator->FrameCount)
                        animator->FrameIndex = animator->FrameLoop;
                    animator->FrameDuration = animator->StartFrame[animator->FrameIndex].Duration;
                }
            }
        }
    }
}
Hitbox* Animator_GetHitbox(Animator* animator, int hitbox) {
    if (animator && animator->StartFrame)
        return &animator->StartFrame[animator->FrameIndex].Hitboxes[hitbox & 7];
    return NULL;
}
int Animator_GetFrameID(Animator* animator) {
    if (animator && animator->StartFrame)
        return animator->StartFrame[animator->FrameIndex].ID;
    return 0;
}

namespace GameLinker {
    HatchFunctionSet HatchFuncs;
    ServicesFunctionSet ServiceFuncs;

    Class ClassList[MAX_CLASSES];
    int   ClassCount = 0;

    void* GameLogicSharedObject = NULL;

    void Init() {
        ClassCount = 0;
        GameLogicSharedObject = NULL;

        Game::State.Views = Graphics::Views;
        Game::State.Inputs = Input::PadInputs;
        Game::State.TouchInputs = Input::TouchInputs;

        memset(&HatchFuncs, 0, sizeof(HatchFuncs));

        memcpy(&ServiceFuncs, &Services::Service, sizeof(ServiceFuncs));

        // Setup functions
        HatchFuncs.AllocateGlobals = [](Globals** globals, size_t size) -> void {
            Memory::Alloc(globals, size, Memory::MEMPOOL_STAGE, true);
        };

        HatchFuncs.Stage.MatchCurrentStageName = [](CString stageName) -> bool {
            return strcmp(stageName, Game::State.Scenes[Game::State.CurrentSceneIndex].Zone) == 0;
        };

        HatchFuncs.Stage.GetCurrentStageName = []() -> CString {
            return Game::State.Scenes[Game::State.CurrentSceneIndex].Zone;
        };

        HatchFuncs.Entity.Get = Scene::Get;
        HatchFuncs.Entity.GetIndex = Scene::GetIndex;
        HatchFuncs.Entity.GetFromDrawGroup = Scene::GetFromDrawGroup;
        HatchFuncs.Entity.GetIndexFromDrawGroup = Scene::GetIndexFromDrawGroup;
        HatchFuncs.Entity.Reset = Scene::Reset;
        HatchFuncs.Entity.ResetAtIndex = Scene::ResetAtIndex;
        HatchFuncs.Entity.Create = Scene::Create;
        HatchFuncs.Entity.Copy = Scene::Copy;
        HatchFuncs.Entity.Move = Scene::Move;
        HatchFuncs.Entity.IsOnScreen = Scene::IsOnScreen;
        HatchFuncs.Entity.IsPointOnScreen = Scene::IsPointOnScreen;

        HatchFuncs.Scene.SetNextFromCategory = [](CString categoryName, CString sceneName) -> void {
            Hash categoryHash = MD5_HashString(categoryName);
            Hash sceneHash = MD5_HashString(sceneName);

            for (int i = 0; i < Game::State.CategoryCount; i++) {
                auto category = &Game::State.Categories[i];
                if (category->NameHash == categoryHash) {
                    if (sceneName[0] == '\0') {
                        Game::State.CurrentSceneIndex = category->FirstSceneIndex;
                        return;
                    }

                    for (int s = category->FirstSceneIndex; s <= category->LastSceneIndex; s++) {
                        auto scene = &Game::State.Scenes[s];
                        if (scene->NameHash == sceneHash) {
                            Game::State.CurrentSceneIndex = s;
                            return;
                        }
                    }
                    break;
                }
            }
        };
        HatchFuncs.Scene.GotoNext = []() -> void {
            Game::State.EngineState = ENGINESTATE_SCENELOAD;
        };

        HatchFuncs.Collision.EntitiesAABB = Collision::EntitiesAABB;
        HatchFuncs.Collision.EntitiesCircular = Collision::EntitiesCircular;
        HatchFuncs.Collision.EntitiesSolid = Collision::EntitiesSolid;
        HatchFuncs.Collision.EntitiesPlatform = Collision::EntitiesPlatform;
        HatchFuncs.Collision.ApplyTile360 = Collision::C360Movement;
        HatchFuncs.Collision.TileHit = Collision::TileHit;
        HatchFuncs.Collision.TileGrip = Collision::TileGrip;

        HatchFuncs.UpdateBounds.Add = [](Vector2* focusPosition, int focusRangeX, int focusRangeY, bool isPremultipliedCoords) -> void {
            int c = Scene::UpdateBoundCount;
            if (c < MAX_VIEWPORTS) {
                Scene::UpdateBounds[c].Focus = focusPosition;
                Scene::UpdateBounds[c].Range.X = focusRangeX;
                Scene::UpdateBounds[c].Range.Y = focusRangeY;
                Scene::UpdateBounds[c].IsPremultipliedCoords = isPremultipliedCoords;
                Scene::UpdateBoundCount++;
            }
        };
        HatchFuncs.UpdateBounds.ClearAll = []() -> void {
            Scene::UpdateBoundCount = 0;
        };

        HatchFuncs.Sprites.ConvertStringToSpriteText = [](Resource spriteIndex, int animIndex, String* string) -> void {
            if (spriteIndex < 0 || spriteIndex >= MAX_SPRITES)
                return;
            if (!string)
                return;
            if (animIndex < 0 || animIndex >= Resources::ResourceSprites[spriteIndex].SpriteData.AnimationCount)
                return;

            auto animEntry = &Resources::ResourceSprites[spriteIndex].SpriteData.Animations[animIndex];
            if (animEntry->FrameCount == 0)
                return;

            for (int i = 0; i < string->Length; i++) {
                auto character = string->Text[i];

                string->Text[i] = -1;

                auto frame = &Resources::ResourceSprites[spriteIndex].SpriteData.Frames[animEntry->StartFrameIndex];
                for (int f = 0; f < animEntry->FrameCount; f++) {
                    if (character == frame->ID) {
                        string->Text[i] = f;
                        break;
                    }
                    frame++;
                }
            }
        };
        HatchFuncs.Sprites.MeasureSpriteTextWidth = [](Resource spriteIndex, int animIndex, String* string, int startIndex, int endIndex, int spacing) -> int {
            if (spriteIndex < 0 || spriteIndex >= MAX_SPRITES)
                return 0;
            if (!string)
                return 0;
            if (animIndex < 0 || animIndex >= Resources::ResourceSprites[spriteIndex].SpriteData.AnimationCount)
                return 0;

            auto animEntry = &Resources::ResourceSprites[spriteIndex].SpriteData.Animations[animIndex];
            if (animEntry->FrameCount == 0)
                return 0;

            if (startIndex > 0)
                startIndex = M_MIN(startIndex, string->Length - 1);
            else
                startIndex = 0;

            if (endIndex > 0)
                endIndex = M_MIN(endIndex, string->Length);
            else
                endIndex = string->Length;

            int width = 0;
            for (int i = startIndex; i < endIndex; i++) {
                auto character = string->Text[i];
                if (character < 0 || character >= animEntry->FrameCount)
                    continue;

                auto frame = &Resources::ResourceSprites[spriteIndex].SpriteData.Frames[animEntry->StartFrameIndex + character];
                width += frame->Width + spacing;
            }

            return width;
        };

        HatchFuncs.Animator.Set = Animator_Set;
        HatchFuncs.Animator.Set3D = Animator_Set3D;
        HatchFuncs.Animator.Update = Animator_Update;
        HatchFuncs.Animator.GetHitbox = Animator_GetHitbox;
        HatchFuncs.Animator.GetFrameID = Animator_GetFrameID;

        HatchFuncs.View3D.SetAmbientLighting = Graphics::View3D_SetAmbientLighting;
        HatchFuncs.View3D.SetDiffuseLighting = Graphics::View3D_SetDiffuseLighting;
        HatchFuncs.View3D.SetSpecularLighting = Graphics::View3D_SetSpecularLighting;
        HatchFuncs.View3D.DrawBegin = Graphics::View3D_DrawBegin;
        HatchFuncs.View3D.DrawFinish = Graphics::View3D_DrawFinish;
        HatchFuncs.View3D.DrawModel = Graphics::View3D_DrawModel;

        HatchFuncs.Draw.Sprite = Graphics::DrawSprite;
        HatchFuncs.Draw.Animation = Graphics::DrawAnimation;
        HatchFuncs.Draw.Image = Graphics::DrawImage;
        HatchFuncs.Draw.SpriteText = Graphics::DrawSpriteText;

        HatchFuncs.Draw.Tile = Graphics::DrawTile;
        HatchFuncs.Draw.CopyImageToTiles = Graphics::CopyImageToTiles;
        HatchFuncs.Draw.SetDrawToScreen = [](bool dTS) -> void {
            Graphics::DrawToScreen = dTS;
        };

        HatchFuncs.Draw.Rectangle = Graphics::DrawRectangle;
        HatchFuncs.Draw.Triangle = Graphics::DrawTriangle;
        HatchFuncs.Draw.Line = Graphics::DrawLine;
        HatchFuncs.Draw.Circle = Graphics::DrawCircle;
        HatchFuncs.Draw.CircleStroke = Graphics::DrawCircleStroke;
        HatchFuncs.Draw.Ring = Graphics::DrawRing;
        HatchFuncs.Draw.Ellipse = Graphics::DrawEllipse;
        HatchFuncs.Draw.Polygon = Graphics::DrawPolygon;
        HatchFuncs.Draw.PolygonBlend = Graphics::DrawPolygonBlend;
        HatchFuncs.Draw.FadeScreen = Graphics::FadeScreen;

        HatchFuncs.Palette.Load = Graphics::PaletteLoad;
        HatchFuncs.Palette.GetColor = Graphics::PaletteGetColor;
        HatchFuncs.Palette.SetColor = Graphics::PaletteSetColor;
        HatchFuncs.Palette.MixPalettes = Graphics::PaletteMixPalettes;
        HatchFuncs.Palette.RotateColorsLeft = Graphics::PaletteRotateColorsLeft;
        HatchFuncs.Palette.RotateColorsRight = Graphics::PaletteRotateColorsRight;
        HatchFuncs.Palette.CopyColors = Graphics::PaletteCopyColors;
        HatchFuncs.Palette.SetPaletteIndexLines = Graphics::PaletteSetPaletteIndexLines;

        HatchFuncs.Class.Add = Classes::Add;
        HatchFuncs.Class.CreateGlobalClass = Classes::CreateGlobalClass;
        HatchFuncs.Class.SetupAttribute = Classes::SetupAttribute;
        HatchFuncs.Class.AddEnumValue = [](CString name, int value) -> void { };
        HatchFuncs.Class.SearchEntity = Scene::FindNextClassEntity;
        HatchFuncs.Class.SearchInteractableEntity = Scene::FindNextClassEntityInteractable;
        HatchFuncs.Class.EndSearch = Scene::FindNextClassEntityBreak;

        HatchFuncs.Resources.LoadSprite = Resources::LoadSprite;
        HatchFuncs.Resources.LoadImage = Resources::LoadImage;
        HatchFuncs.Resources.LoadMesh = Resources::LoadMesh;
        HatchFuncs.Resources.LoadView3D = Resources::LoadView3D;
        HatchFuncs.Resources.LoadSound = Resources::LoadSound;

        HatchFuncs.Math.Sin256 = [](int n) -> int { n &= 0xFF; return Math::SinTbl_0x100[n]; };
        HatchFuncs.Math.Cos256 = [](int n) -> int { n &= 0xFF; return Math::CosTbl_0x100[n]; };
        HatchFuncs.Math.Tan256 = [](int n) -> int { n &= 0xFF; return Math::TanTbl_0x100[n]; };
        HatchFuncs.Math.Asin256 = [](int n) -> int { n &= 0xFF; return Math::ASinTbl_0x100[n]; };
        HatchFuncs.Math.Acos256 = [](int n) -> int { n &= 0xFF; return Math::ACosTbl_0x100[n]; };
        HatchFuncs.Math.Atan256 = Math::ATan;

        HatchFuncs.Math.Sin512 = [](int n) -> int { n &= 0x1FF; return Math::SinTbl_0x200[n]; };
        HatchFuncs.Math.Cos512 = [](int n) -> int { n &= 0x1FF; return Math::CosTbl_0x200[n]; };
        HatchFuncs.Math.Tan512 = [](int n) -> int { n &= 0x1FF; return Math::TanTbl_0x200[n]; };
        HatchFuncs.Math.Asin512 = [](int n) -> int { n &= 0x1FF; return Math::ASinTbl_0x200[n]; };
        HatchFuncs.Math.Acos512 = [](int n) -> int { n &= 0x1FF; return Math::ACosTbl_0x200[n]; };

        HatchFuncs.Math.Sin1024 = [](int n) -> int { n &= 0x3FF; return Math::SinTbl_0x400[n]; };
        HatchFuncs.Math.Cos1024 = [](int n) -> int { n &= 0x3FF; return Math::CosTbl_0x400[n]; };
        HatchFuncs.Math.Tan1024 = [](int n) -> int { n &= 0x3FF; return Math::TanTbl_0x400[n]; };
        HatchFuncs.Math.Asin1024 = [](int n) -> int { n &= 0x3FF; return Math::ASinTbl_0x400[n]; };
        HatchFuncs.Math.Acos1024 = [](int n) -> int { n &= 0x3FF; return Math::ACosTbl_0x400[n]; };

        HatchFuncs.Math.Sqrt = Math::Sqrt;
        HatchFuncs.Math.Distance = [](int x1, int y1, int x2, int y2) -> int {
            return (int)sqrt((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1));
        };
        HatchFuncs.Math.Abs = [](int n) -> int { return M_ABS(n); };
        HatchFuncs.Math.Min = [](int a, int b) -> int { return M_MIN(a, b); };
        HatchFuncs.Math.Max = [](int a, int b) -> int { return M_MAX(a, b); };
        HatchFuncs.Math.Clamp = [](int n, int min, int max) -> int { return M_CLAMP(n, min, max); };
        HatchFuncs.Math.GetRandom = Math::RandomRange;
        HatchFuncs.Math.SetRandomSeed = Math::RandomSetSeed;
        HatchFuncs.Math.GetRandomSeeded = Math::RandomRangeSeeded;

        HatchFuncs.Matrix.Identity = [](Matrix4x4* matrix) -> void {
            matrix->Column[0][0] = 0x100;
            matrix->Column[1][0] = 0;
            matrix->Column[2][0] = 0;
            matrix->Column[3][0] = 0;

            matrix->Column[0][1] = 0;
            matrix->Column[1][1] = 0x100;
            matrix->Column[2][1] = 0;
            matrix->Column[3][1] = 0;

            matrix->Column[0][2] = 0;
            matrix->Column[1][2] = 0;
            matrix->Column[2][2] = 0x100;
            matrix->Column[3][2] = 0;

            matrix->Column[0][3] = 0;
            matrix->Column[1][3] = 0;
            matrix->Column[2][3] = 0;
            matrix->Column[3][3] = 0x100;
        };
        HatchFuncs.Matrix.Multiply = [](Matrix4x4* out, Matrix4x4* a, Matrix4x4* b) -> void {
            int b0, b1, b2, b3;
            int a00 = a->Column[0][0], a01 = a->Column[0][1], a02 = a->Column[0][2], a03 = a->Column[0][3];
            int a10 = a->Column[1][0], a11 = a->Column[1][1], a12 = a->Column[1][2], a13 = a->Column[1][3];
            int a20 = a->Column[2][0], a21 = a->Column[2][1], a22 = a->Column[2][2], a23 = a->Column[2][3];
            int a30 = a->Column[3][0], a31 = a->Column[3][1], a32 = a->Column[3][2], a33 = a->Column[3][3];

            // Cache only the current line of the second matrix
            b0 = b->Column[0][0]; b1 = b->Column[0][1]; b2 = b->Column[0][2]; b3 = b->Column[0][3];
            out->Column[0][0] = (b0 * a00 + b1 * a10 + b2 * a20 + b3 * a30) >> 8;
            out->Column[0][1] = (b0 * a01 + b1 * a11 + b2 * a21 + b3 * a31) >> 8;
            out->Column[0][2] = (b0 * a02 + b1 * a12 + b2 * a22 + b3 * a32) >> 8;
            out->Column[0][3] = (b0 * a03 + b1 * a13 + b2 * a23 + b3 * a33) >> 8;

            b0 = b->Column[1][0]; b1 = b->Column[1][1]; b2 = b->Column[1][2]; b3 = b->Column[1][3];
            out->Column[1][0] = (b0 * a00 + b1 * a10 + b2 * a20 + b3 * a30) >> 8;
            out->Column[1][1] = (b0 * a01 + b1 * a11 + b2 * a21 + b3 * a31) >> 8;
            out->Column[1][2] = (b0 * a02 + b1 * a12 + b2 * a22 + b3 * a32) >> 8;
            out->Column[1][3] = (b0 * a03 + b1 * a13 + b2 * a23 + b3 * a33) >> 8;

            b0 = b->Column[2][0]; b1 = b->Column[2][1]; b2 = b->Column[2][2]; b3 = b->Column[2][3];
            out->Column[2][0] = (b0 * a00 + b1 * a10 + b2 * a20 + b3 * a30) >> 8;
            out->Column[2][1] = (b0 * a01 + b1 * a11 + b2 * a21 + b3 * a31) >> 8;
            out->Column[2][2] = (b0 * a02 + b1 * a12 + b2 * a22 + b3 * a32) >> 8;
            out->Column[2][3] = (b0 * a03 + b1 * a13 + b2 * a23 + b3 * a33) >> 8;

            b0 = b->Column[3][0]; b1 = b->Column[3][1]; b2 = b->Column[3][2]; b3 = b->Column[3][3];
            out->Column[3][0] = (b0 * a00 + b1 * a10 + b2 * a20 + b3 * a30) >> 8;
            out->Column[3][1] = (b0 * a01 + b1 * a11 + b2 * a21 + b3 * a31) >> 8;
            out->Column[3][2] = (b0 * a02 + b1 * a12 + b2 * a22 + b3 * a32) >> 8;
            out->Column[3][3] = (b0 * a03 + b1 * a13 + b2 * a23 + b3 * a33) >> 8;
        };
        HatchFuncs.Matrix.Translate = [](Matrix4x4* matrix, int x, int y, int z, bool resetToIdentity) -> void {
            if (resetToIdentity) {
                matrix->Column[0][0] = 0x100;
                matrix->Column[1][0] = 0;
                matrix->Column[2][0] = 0;
                matrix->Column[3][0] = 0;

                matrix->Column[0][1] = 0;
                matrix->Column[1][1] = 0x100;
                matrix->Column[2][1] = 0;
                matrix->Column[3][1] = 0;

                matrix->Column[0][2] = 0;
                matrix->Column[1][2] = 0;
                matrix->Column[2][2] = 0x100;
                matrix->Column[3][2] = 0;

                matrix->Column[3][3] = 0x100;
            }

            matrix->Column[0][3] = x >> 8;
            matrix->Column[1][3] = y >> 8;
            matrix->Column[2][3] = z >> 8;
        };
        HatchFuncs.Matrix.IdentityScale = [](Matrix4x4* matrix, int x, int y, int z) -> void {
            matrix->Column[0][0] = x;
            matrix->Column[1][0] = 0;
            matrix->Column[2][0] = 0;
            matrix->Column[3][0] = 0;
            matrix->Column[0][1] = 0;
            matrix->Column[1][1] = y;
            matrix->Column[2][1] = 0;
            matrix->Column[3][1] = 0;
            matrix->Column[0][2] = 0;
            matrix->Column[1][2] = 0;
            matrix->Column[2][2] = z;
            matrix->Column[3][2] = 0;
            matrix->Column[0][3] = 0;
            matrix->Column[1][3] = 0;
            matrix->Column[2][3] = 0;
            matrix->Column[3][3] = 0x100;
        };
        HatchFuncs.Matrix.IdentityRotationX = [](Matrix4x4* matrix, int x) -> void {
            x &= 0x3FF;
            int sin = Math::SinTbl_0x400[x] >> 2;
            int cos = Math::CosTbl_0x400[x] >> 2;
            matrix->Column[0][0] = 0x100;
            matrix->Column[1][0] = 0;
            matrix->Column[2][0] = 0;
            matrix->Column[3][0] = 0;
            matrix->Column[0][1] = 0;
            matrix->Column[1][1] = cos;
            matrix->Column[2][1] = sin;
            matrix->Column[3][1] = 0;
            matrix->Column[0][2] = 0;
            matrix->Column[1][2] = -sin;
            matrix->Column[2][2] = cos;
            matrix->Column[3][2] = 0;
            matrix->Column[0][3] = 0;
            matrix->Column[1][3] = 0;
            matrix->Column[2][3] = 0;
            matrix->Column[3][3] = 0x100;
        };
        HatchFuncs.Matrix.IdentityRotationY = [](Matrix4x4* matrix, int y) -> void {
            y &= 0x3FF;
            int sin = Math::SinTbl_0x400[y] >> 2;
            int cos = Math::CosTbl_0x400[y] >> 2;
            matrix->Column[0][0] = cos;
            matrix->Column[1][0] = 0;
            matrix->Column[2][0] = sin;
            matrix->Column[3][0] = 0;
            matrix->Column[0][1] = 0;
            matrix->Column[1][1] = 0x100;
            matrix->Column[2][1] = 0;
            matrix->Column[3][1] = 0;
            matrix->Column[0][2] = -sin;
            matrix->Column[1][2] = 0;
            matrix->Column[2][2] = cos;
            matrix->Column[3][2] = 0;
            matrix->Column[0][3] = 0;
            matrix->Column[1][3] = 0;
            matrix->Column[2][3] = 0;
            matrix->Column[3][3] = 0x100;
        };
        HatchFuncs.Matrix.IdentityRotationZ = [](Matrix4x4* matrix, int z) -> void {
            z &= 0x3FF;
            int sin = Math::SinTbl_0x400[z] >> 2;
            int cos = Math::CosTbl_0x400[z] >> 2;
            matrix->Column[0][0] = cos;
            matrix->Column[1][0] = -sin;
            matrix->Column[2][0] = 0;
            matrix->Column[3][0] = 0;
            matrix->Column[0][1] = sin;
            matrix->Column[1][1] = cos;
            matrix->Column[2][1] = 0;
            matrix->Column[3][1] = 0;
            matrix->Column[0][2] = 0;
            matrix->Column[1][2] = 0;
            matrix->Column[2][2] = 0x100;
            matrix->Column[3][2] = 0;
            matrix->Column[0][3] = 0;
            matrix->Column[1][3] = 0;
            matrix->Column[2][3] = 0;
            matrix->Column[3][3] = 0x100;
        };
        HatchFuncs.Matrix.IdentityRotationXYZ = [](Matrix4x4* matrix, int x, int y, int z) -> void {
            x &= 0x3FF;
            y &= 0x3FF;
            z &= 0x3FF;
            int sinX = Math::SinTbl_0x400[x] >> 2;
            int cosX = Math::CosTbl_0x400[x] >> 2;
            int sinY = Math::SinTbl_0x400[y] >> 2;
            int cosY = Math::CosTbl_0x400[y] >> 2;
            int sinZ = Math::SinTbl_0x400[z] >> 2;
            int cosZ = Math::CosTbl_0x400[z] >> 2;
            int sinXY = sinX * sinY >> 8;
            matrix->Column[0][0] = (cosY * cosZ >> 8) + (sinZ * sinXY >> 8);
            matrix->Column[1][0] = (cosY * sinZ >> 8) - (cosZ * sinXY >> 8);
            matrix->Column[2][0] = cosX * sinY >> 8;
            matrix->Column[3][0] = 0;
            matrix->Column[0][1] = -(cosX * sinZ) >> 8;
            matrix->Column[1][1] = cosX * cosZ >> 8;
            matrix->Column[2][1] = 0;
            matrix->Column[3][1] = 0;

            int sincosXY = sinX * cosY >> 8;
            matrix->Column[0][2] = (sinZ * sincosXY >> 8) - (sinY * cosZ >> 8);
            matrix->Column[1][2] = (-sinZ * sinY >> 8) - (cosZ * sincosXY >> 8);
            matrix->Column[2][2] = cosX * cosY >> 8;
            matrix->Column[3][2] = 0;
            matrix->Column[0][3] = 0;
            matrix->Column[1][3] = 0;
            matrix->Column[2][3] = 0;
            matrix->Column[3][3] = 0x100;
        };

        HatchFuncs.Audio.PlayStream = Audio::PlayStream;
        HatchFuncs.Audio.PlaySoundFX = Audio::PlaySoundFX;
        HatchFuncs.Audio.StopSoundFX = Audio::StopSoundFX;
        HatchFuncs.Audio.IsSoundFXPlaying = Audio::IsSoundFXPlaying;
        HatchFuncs.Audio.PlaybackAlter = Audio::PlaybackAlter;
        HatchFuncs.Audio.PlaybackIsValid = Audio::PlaybackIsValid;
        HatchFuncs.Audio.PlaybackGetSamplePosition = Audio::PlaybackGetSamplePosition;
        HatchFuncs.Audio.PlaybackStop = Audio::PlaybackStop;
        HatchFuncs.Audio.PlaybackPause = Audio::PlaybackPause;
        HatchFuncs.Audio.PlaybackResume = Audio::PlaybackResume;
        HatchFuncs.Audio.PlaybackPauseAll = Audio::PlaybackPauseAll;
        HatchFuncs.Audio.PlaybackResumeAll = Audio::PlaybackResumeAll;

        HatchFuncs.Video.PlayStream = Video::PlayStream;

        HatchFuncs.Layer.GetIndexFromLayer = Scene::GetLayerIndexByLayer;
        HatchFuncs.Layer.GetIndexFromName = Scene::GetLayerIndexByName;
        HatchFuncs.Layer.GetLayerFromIndex = Scene::GetLayerByIndex;
        HatchFuncs.Layer.GetLayerFromName = Scene::GetLayerByName;
        HatchFuncs.Layer.GetSize = Scene::GetLayerSize;

        HatchFuncs.Layer.GetTile = [](Layer* layer, int x, int y) -> Tile {
            return layer->Tiles[x + (y << layer->WidthInBits)];
        };
        HatchFuncs.Layer.GetTileLine = [](Layer* layer, int y) -> Tile* {
            return &layer->Tiles[y << layer->WidthInBits];
        };

        HatchFuncs.TileConfig.GetAngle = [](int tileID, int plane, int side) -> Uint8 {
            switch (side) {
            case 0: return Collision::TileCfg[plane][tileID].AngleTop;
            case 1: return Collision::TileCfg[plane][tileID].AngleLeft;
            case 2: return Collision::TileCfg[plane][tileID].AngleBottom;
            case 3: return Collision::TileCfg[plane][tileID].AngleRight;
            }
            return 0x00;
            
        };
        HatchFuncs.TileConfig.SetAngle = [](int tileID, int plane, int side, Uint8 angle) -> void { 
            switch (side) {
            case 0: Collision::TileCfg[plane][tileID].AngleTop = angle;
            case 1: Collision::TileCfg[plane][tileID].AngleLeft = angle;
            case 2: Collision::TileCfg[plane][tileID].AngleBottom = angle;
            case 3: Collision::TileCfg[plane][tileID].AngleRight = angle;
            }
        };
        HatchFuncs.TileConfig.GetBehaviorFlag = [](int tileID, int plane) -> Uint8 {
            return Collision::TileCfg[plane][tileID].Behavior;
        };
        HatchFuncs.TileConfig.SetBehaviorFlag = [](int tileID, int plane, Uint8 flag) -> void {
            Collision::TileCfg[plane][tileID].Behavior = flag;
        };

        HatchFuncs.DrawGroup.AddEntity = [](int drawGroup, Entity* entity) -> void {
            auto& i = Graphics::DrawGroups[drawGroup].EntityCount;
            Graphics::DrawGroups[drawGroup].EntityIndices[i++] = Scene::GetIndex(entity);
        };
        HatchFuncs.DrawGroup.ReorderEntities = [](int drawGroup, int entityIndexAbove, int entityIndexBelow, int maxEntityCount) -> void {
            auto* d = &Graphics::DrawGroups[drawGroup];
            maxEntityCount = M_CLAMP(maxEntityCount, 0, d->EntityCount);

            int slotBelow = -1;
            int slotAbove = -1;
            for (int i = 0; i < maxEntityCount; i++) {
                int entityIndex = d->EntityIndices[i];
                if (entityIndexBelow == entityIndex)
                    slotBelow = i;
                if (entityIndexAbove == entityIndex)
                    slotAbove = i;
            }

            if (slotBelow != -1 && slotAbove != -1 && slotAbove < slotBelow) {
                auto temp = d->EntityIndices[slotBelow];
                d->EntityIndices[slotBelow] = d->EntityIndices[slotAbove];
                d->EntityIndices[slotAbove] = temp;
            }
        };
        HatchFuncs.DrawGroup.SetPrefixFunction = [](int drawGroup, void (*prefixFunction)()) -> void {
            Graphics::DrawGroups[drawGroup].PrefixFunction = prefixFunction;
        };
        HatchFuncs.DrawGroup.SetSorting = [](int drawGroup, bool doSort) -> void {
            Graphics::DrawGroups[drawGroup].EntityDepthSortingEnabled = doSort;
        };

        HatchFuncs.View.SetClip = [](int viewIndex, int x, int y, int w, int h) -> void {
            View* view = &Graphics::Views[viewIndex];
            view->ClipStartX = M_CLAMP(x, 0, view->Width);
            view->ClipStartY = M_CLAMP(y, 0, view->Height);
            view->ClipEndX = M_CLAMP(x + w, 0, view->Width);
            view->ClipEndY = M_CLAMP(y + h, 0, view->Height);
        };
        HatchFuncs.View.ResetClip = [](int viewIndex) -> void {
            View* view = &Graphics::Views[viewIndex];
            view->ClipStartX = 0;
            view->ClipStartY = 0;
            view->ClipEndX = view->Width;
            view->ClipEndY = view->Height;
        };

        HatchFuncs.String.Init = Strings::Init;
        HatchFuncs.String.FromUnicode = Strings::FromUnicode;
        HatchFuncs.String.FromCString = Strings::FromCString;
        HatchFuncs.String.FromResource = Strings::FromResource;
        HatchFuncs.String.Copy = Strings::Copy;
        HatchFuncs.String.Concat = Strings::Concat;
        HatchFuncs.String.Match = Strings::Match;
        HatchFuncs.String.ToCString = Strings::ToCString;

        #ifdef _DEBUG
        void** funcList;
        int funcsFilled, funcsTotal;

        funcList = (void**)&HatchFuncs;
        funcsFilled = funcsTotal = 0;
        for (int i = 0; i < sizeof(HatchFuncs) / sizeof(void*); i++) {
            if (*funcList != NULL)
                funcsFilled++;
            funcsTotal++;
            funcList++;
        }
        fprintf(stdout, "Hatch: %d / %d functions added.\n", funcsFilled, funcsTotal);

        funcList = (void**)&ServiceFuncs;
        funcsFilled = funcsTotal = 0;
        for (int i = 0; i < sizeof(ServiceFuncs) / sizeof(void*); i++) {
            if (*funcList != NULL)
                funcsFilled++;
            funcsTotal++;
            funcList++;
        }
        fprintf(stdout, "Services: %d / %d functions added.\n", funcsFilled, funcsTotal);
        #endif
    }
}
