#include PLATFORM_SETTINGS
#include <Hatch/Primitives.h>
#include <Hatch/Types.h>

#include <Hatch/Collision.h>

#include <Hatch/Math.h>
#include <Hatch/Scene.h>

namespace Collision {
    // Hit is for when you're above ground.
    // Grip is for when want to stay on ground.
    Entity* entity360;
    Hitbox  outerHitbox360;
    Hitbox  innerHitbox360;
    int     granularity360;
    int     tolerance360;
    int     yoffset360;

    TileConfig TileCfg[2][MAX_TILE_COUNT << 2];

    const int ANGLE_OVERRIDE_CHECK = -0x79;

    bool EntitiesAABB(Entity* entityA, Hitbox* hitboxA, Entity* entityB, Hitbox* hitboxB) {
        int temp;
        bool collided = false;
        if (!entityA || !entityB || !hitboxA || !hitboxB)
            return collided;


        switch (entityA->FlipFlag) {
            case FLIPXY_X:
                temp = -hitboxA->Left;
                hitboxA->Left = -hitboxA->Right;
                hitboxA->Right = temp;
                break;
            case FLIPXY_XY:
                temp = -hitboxA->Left;
                hitboxA->Left = -hitboxA->Right;
                hitboxA->Right = temp;
            case FLIPXY_Y:
                temp = -hitboxA->Top;
                hitboxA->Top = -hitboxA->Bottom;
                hitboxA->Bottom = temp;
                break;
        }
        switch (entityA->FlipFlag) {
            case FLIPXY_X:
                temp = -hitboxB->Left;
                hitboxB->Left = -hitboxB->Right;
                hitboxB->Right = temp;
                break;
            case FLIPXY_XY:
                temp = -hitboxB->Left;
                hitboxB->Left = -hitboxB->Right;
                hitboxB->Right = temp;
            case FLIPXY_Y:
                temp = -hitboxB->Top;
                hitboxB->Top = -hitboxB->Bottom;
                hitboxB->Bottom = temp;
                break;
        }

        if (entityA->Position.X.Whole + hitboxA->Left >= entityB->Position.X.Whole + hitboxB->Right ||
            entityA->Position.X.Whole + hitboxA->Right <= entityB->Position.X.Whole + hitboxB->Left ||
            entityA->Position.Y.Whole + hitboxA->Top >= entityB->Position.Y.Whole + hitboxB->Bottom ||
            entityA->Position.Y.Whole + hitboxA->Bottom <= entityB->Position.Y.Whole + hitboxB->Top) {
            collided = false;
        }
        else {
            collided = true;
        }

        switch (entityA->FlipFlag) {
            case FLIPXY_X:
                temp = -hitboxA->Left;
                hitboxA->Left = -hitboxA->Right;
                hitboxA->Right = temp;
                break;
            case FLIPXY_XY:
                temp = -hitboxA->Left;
                hitboxA->Left = -hitboxA->Right;
                hitboxA->Right = temp;
            case FLIPXY_Y:
                temp = -hitboxA->Top;
                hitboxA->Top = -hitboxA->Bottom;
                hitboxA->Bottom = temp;
                break;
        }
        switch (entityA->FlipFlag) {
            case FLIPXY_X:
                temp = -hitboxB->Left;
                hitboxB->Left = -hitboxB->Right;
                hitboxB->Right = temp;
                break;
            case FLIPXY_XY:
                temp = -hitboxB->Left;
                hitboxB->Left = -hitboxB->Right;
                hitboxB->Right = temp;
            case FLIPXY_Y:
                temp = -hitboxB->Top;
                hitboxB->Top = -hitboxB->Bottom;
                hitboxB->Bottom = temp;
                break;
        }

        return collided;
    }
    bool EntitiesCircular(Entity* entityA, Subpixels radiusA, Entity* entityB, Subpixels radiusB) {
        int x = entityA->Position.X.Whole - entityB->Position.X.Whole;
        int y = entityA->Position.Y.Whole - entityB->Position.Y.Whole;
        x *= x;
        y *= y;
        return x + y <= (radiusA.Whole + radiusB.Whole) * (radiusA.Whole + radiusB.Whole);
    }
    int  EntitiesSolid(Entity* entitySolid, Hitbox* hitboxSolid, Entity* entity, Hitbox* hitbox, bool adjustSpeeds) {
        Vector2 initPos = entity->Position;
        Vector2 solidPos = entitySolid->Position;
        Vector2 otherPos = entity->Position;

        int collideSideHori = COLLSIDE_NONE;
        int collideSideVert = COLLSIDE_NONE;

        // NOTE: Keep this.
        // if ( other_X <= (sourceHitbox->Right + sourceHitbox->Left + 2 * source_X) >> 1 )
        // if ( other_X <= (sourceHitbox->Right + sourceHitbox->Left) / 2 + source_X )
        // if ( other_X <= source_X + (sourceHitbox->Right + sourceHitbox->Left) / 2 )
        // if entity->X <= entitySolid->X + entitySolid->HitboxCenterX

        // Check squeezed vertically
        if (solidPos.Y.Whole + hitboxSolid->Top + 1 < initPos.Y.Whole + hitbox->Bottom &&
            solidPos.Y.Whole + hitboxSolid->Bottom - 1 > initPos.Y.Whole + hitbox->Top) {
            // if entity->X <= entitySolid->X + entitySolid->HitboxCenterX
            if (otherPos.X.Whole <= solidPos.X.Whole) {
                if (otherPos.X.Whole + hitbox->Right >= solidPos.X.Whole + hitboxSolid->Left) {
                    collideSideHori = COLLSIDE_LEFT;
                    otherPos.X = entitySolid->Position.X + ((hitboxSolid->Left - hitbox->Right) << 16);
                }
            }
            else {
                if (otherPos.X.Whole + hitbox->Left < solidPos.X.Whole + hitboxSolid->Right) {
                    collideSideHori = COLLSIDE_RIGHT;
                    otherPos.X = entitySolid->Position.X + ((hitboxSolid->Right - hitbox->Left) << 16);
                }
            }
        }

        // Check squeezed horizontally
        if (solidPos.X.Whole + hitboxSolid->Left + 1 < initPos.X.Whole + hitbox->Right &&
            solidPos.X.Whole + hitboxSolid->Right - 1 > initPos.X.Whole + hitbox->Left) {
            // if entity->Y <= entitySolid->Y + entitySolid->HitboxCenterY
            if (otherPos.Y.Whole <= solidPos.Y.Whole) {
                if (otherPos.Y.Whole + hitbox->Bottom >= solidPos.Y.Whole + hitboxSolid->Top) {
                    collideSideVert = COLLSIDE_TOP;
                    otherPos.Y.Whole = entitySolid->Position.Y.Whole + (hitboxSolid->Top - hitbox->Bottom);
                }
            }
            else {
                if (otherPos.Y.Whole + hitbox->Top < solidPos.Y.Whole + hitboxSolid->Bottom) {
                    collideSideVert = COLLSIDE_BOTTOM;
                    otherPos.Y.Whole = entitySolid->Position.Y.Whole + (hitboxSolid->Bottom - hitbox->Top);
                }
            }
        }

        int deltaSquaredX1 = otherPos.X.Whole - (entity->Position.X.Whole);
        int deltaSquaredX2 = initPos.X.Whole - (entity->Position.X.Whole);
        int deltaSquaredY1 = otherPos.Y.Whole - (entity->Position.Y.Whole);
        int deltaSquaredY2 = initPos.Y.Whole - (entity->Position.Y.Whole);

        if (collideSideHori || collideSideVert) {
            // print
            //     " deltaSquaredX1: " + deltaSquaredX1 +
            //     " deltaSquaredX2: " + deltaSquaredX2 +
            //     " deltaSquaredY1: " + deltaSquaredY1 +
            //     " deltaSquaredY2: " + deltaSquaredY2;
			/*
			printf("collideSideHori: %s collideSideVert: %s\n",
				   collideSideHori == COLLSIDE_LEFT ? "LEFT" :
				   collideSideHori == COLLSIDE_RIGHT ? "RIGHT" :
				   "NONE",
				   collideSideVert == COLLSIDE_TOP ? "TOP" :
				   collideSideVert == COLLSIDE_BOTTOM ? "BOTTOM" :
				   "NONE");
			//*/
            // print
            //     " collideSideHori: " + collideSideHori +
            //     " collideSideVert: " + collideSideVert;
        }

        deltaSquaredX1 *= deltaSquaredX1;
        deltaSquaredX2 *= deltaSquaredX2;
        deltaSquaredY1 *= deltaSquaredY1;
        deltaSquaredY2 *= deltaSquaredY2;

        if (collideSideHori || collideSideVert) {
            if (deltaSquaredX1 + deltaSquaredY2 >= deltaSquaredX2 + deltaSquaredY1) {
                if (collideSideVert || collideSideHori == COLLSIDE_NONE) {
                    entity->Position.X = initPos.X;
                    entity->Position.Y = otherPos.Y;
                    if (adjustSpeeds) {
                        if (collideSideVert == COLLSIDE_TOP) {
                            if (entity->Speed.Y > 0)
                                entity->Speed.Y = 0;

                            if (!entity->Grounded && entity->Speed.Y >= 0) {
                                entity->GroundSpeed = entity->Speed.X;
                                entity->Angle = 0;
                                entity->Grounded = true;
                            }

                            return collideSideVert;
                        }
                        else if (collideSideVert == COLLSIDE_BOTTOM) {
                            if (entity->Speed.Y < 0)
                                entity->Speed.Y = 0;

                            return collideSideVert;
                        }
                    }
                    return collideSideVert;
                }
            }
            else {
                if (collideSideVert && collideSideHori == COLLSIDE_NONE) {
                    entity->Position.X = initPos.X;
                    entity->Position.Y = otherPos.Y;
                    if (adjustSpeeds) {
                        if (collideSideVert == COLLSIDE_TOP) {
                            if (entity->Speed.Y > 0)
                                entity->Speed.Y = 0;

                            if (!entity->Grounded && entity->Speed.Y >= 0) {
                                entity->GroundSpeed = entity->Speed.X;
                                entity->Angle = 0;
                                entity->Grounded = true;
                            }

                            return collideSideVert;
                        }
                        else if (collideSideVert == COLLSIDE_BOTTOM) {
                            if (entity->Speed.Y < 0)
                                entity->Speed.Y = 0;

                            return collideSideVert;
                        }
                    }
                    return collideSideVert;
                }
            }

            entity->Position.X = otherPos.X;
            entity->Position.Y = initPos.Y;
            if (adjustSpeeds) {
                Subpixels v50;
                if (entity->Grounded) {
                    v50 = entity->GroundSpeed;
                    if (entity->AngleMode == 2)
                        v50 = -v50;
                }
                else {
                    v50 = entity->Speed.X;
                }

                if (collideSideHori == COLLSIDE_LEFT) {
                    if (v50 <= 0)
                        return collideSideHori;
                }
                else if (collideSideHori != COLLSIDE_RIGHT || v50 >= 0) {
                    return collideSideHori;
                }

                entity->GroundSpeed = 0;
                entity->Speed.X = 0;
            }
            return collideSideHori;
        }
        return COLLSIDE_NONE;
    }
    bool EntitiesPlatform(Entity* entityPlatform, Hitbox* hitboxPlatform, Entity* entity, Hitbox* hitbox, bool adjustSpeeds) {
        return false;
    }

    void SetGripSensors(Sensor* sensors) {
        switch (entity360->AngleMode) {
            case 0:
                sensors[0].Y =
                sensors[1].Y =
                sensors[2].Y = sensors[4].Y + (outerHitbox360.Bottom << 16);
                sensors[3].Y = sensors[4].Y + yoffset360;

                sensors[0].X = sensors[4].X + (innerHitbox360.Left << 16) - 0x10000;
                sensors[1].X = sensors[4].X;
                sensors[2].X = sensors[4].X + (innerHitbox360.Right << 16);

                if (entity360->GroundSpeed.Full > 0)
                    sensors[3].X = sensors[4].X + (outerHitbox360.Right << 16);
                else
                    sensors[3].X = sensors[4].X + (outerHitbox360.Left << 16) - 0x10000;
                break;
            case 1:
                sensors[0].X =
                sensors[1].X =
                sensors[2].X = sensors[4].X + (outerHitbox360.Bottom << 16);
                sensors[3].X = sensors[4].X + yoffset360;

                sensors[0].Y = sensors[4].Y + (innerHitbox360.Left << 16) - 0x10000;
                sensors[1].Y = sensors[4].Y;
                sensors[2].Y = sensors[4].Y + (innerHitbox360.Right << 16);

                if (entity360->GroundSpeed.Full > 0)
                    sensors[3].Y = sensors[4].Y - (outerHitbox360.Right << 16) - 0x10000;
                else
                    sensors[3].Y = sensors[4].Y - (outerHitbox360.Left << 16);
                break;
            case 2:
                sensors[0].Y =
                sensors[1].Y =
                sensors[2].Y = sensors[4].Y - (outerHitbox360.Bottom << 16) - 0x10000;
                sensors[3].Y = sensors[4].Y - yoffset360;

                sensors[0].X = sensors[4].X + (innerHitbox360.Left << 16) - 0x10000;
                sensors[1].X = sensors[4].X;
                sensors[2].X = sensors[4].X + (innerHitbox360.Right << 16);

                if (entity360->GroundSpeed.Full > 0)
                    sensors[3].X = sensors[4].X - (outerHitbox360.Right << 16) - 0x10000;
                else
                    sensors[3].X = sensors[4].X - (outerHitbox360.Left << 16);
                break;
            case 3:
                sensors[0].X =
                sensors[1].X =
                sensors[2].X = sensors[4].X - (outerHitbox360.Bottom << 16) - 0x10000;
                sensors[3].X = sensors[4].X - yoffset360;

                sensors[0].Y = sensors[4].Y + (innerHitbox360.Left << 16) - 0x10000;
                sensors[1].Y = sensors[4].Y;
                sensors[2].Y = sensors[4].Y + (innerHitbox360.Right << 16);

                if (entity360->GroundSpeed.Full > 0)
                    sensors[3].Y = sensors[4].Y + (outerHitbox360.Right << 16);
                else
                    sensors[3].Y = sensors[4].Y + (outerHitbox360.Left << 16) - 0x10000;
                break;
        }
    }
    void SensorHitFloor(Sensor* sensor) {
        Uint16 layerBit = 1;
        Uint32 solidityMask;

        Layer* layer = Scene::Layers;

        int x = sensor->X >> 16;
        int y = sensor->Y >> 16;

        if (entity360->PlaneIndex == 0)
            solidityMask = ((TILE_COLLA_MASK >> 1) & TILE_COLLA_MASK); // 0x1000;
        else
            solidityMask = ((TILE_COLLB_MASK >> 1) & TILE_COLLB_MASK); // 0x4000;

        while (layerBit < 256) {
            if ((entity360->LayerCollisionFlag & layerBit)) {
                x -= layer->CollideOffset.X.Whole;
                y -= layer->CollideOffset.Y.Whole;

                int width = layer->Width << 4;
                int height = layer->Height << 4;
                int tileY = (y & 0xFFFFFFF0) - 16;
                if (x >= 0 && x < width) {
                    Tile* tile = &layer->Tiles[(x >> 4) | ((tileY >> 4) << layer->WidthInBits)];
                    for (int tileCheck = 0; tileCheck < 3; tileCheck++) {
                        if (tileY >= 0 && tileY < height && *tile != TILE_EMPTY && (*tile & solidityMask)) {
                            int maskHeight = TileCfg[entity360->PlaneIndex][*tile & TILE_FXYID_MASK].CollisionTop[x & 0xF];
                            if (maskHeight >= 0) {
                                maskHeight += tileY;
                                if (y >= maskHeight && M_ABS(y - maskHeight) <= 14) {
                                    sensor->Collided = true;
                                    sensor->Angle = TileCfg[entity360->PlaneIndex][*tile & TILE_FXYID_MASK].AngleTop;
                                    sensor->Y = (maskHeight + layer->CollideOffset.Y.Whole) << 16;
                                    tileCheck = 3;
                                }
                            }
                        }

                        tileY += 16;
                        tile += 1 << layer->WidthInBits;
                    }
                }

                x += layer->CollideOffset.X.Whole;
                y += layer->CollideOffset.Y.Whole;
            }

            layer++;
            layerBit <<= 1;
        }
    }
    void SensorHitLeftWall(Sensor* sensor) {
        Uint16 layerBit = 1;
        Uint32 solidityMask;

        Layer* layer = Scene::Layers;

        int x = sensor->X >> 16;
        int y = sensor->Y >> 16;

        if (entity360->PlaneIndex == 0)
            solidityMask = ((TILE_COLLA_MASK >> 1) & TILE_COLLA_MASK) << 1; // 0x2000;
        else
            solidityMask = ((TILE_COLLB_MASK >> 1) & TILE_COLLB_MASK) << 1; // 0x8000;

        while (layerBit < 256) {
            if ((entity360->LayerCollisionFlag & layerBit)) {
                x -= layer->CollideOffset.X.Whole;
                y -= layer->CollideOffset.Y.Whole;

                int width = layer->Width << 4;
                int height = layer->Height << 4;
                int tileX = (x & 0xFFFFFFF0) - 16;
                if (y >= 0 && y < height) {
                    Tile* tile = &layer->Tiles[(tileX >> 4) | ((y >> 4) << layer->WidthInBits)];
                    for (int tileCheck = 0; tileCheck < 3; tileCheck++) {
                        if (tileX >= 0 && tileX < width && *tile != TILE_EMPTY && (*tile & solidityMask)) {
                            int maskHeight = TileCfg[entity360->PlaneIndex][*tile & TILE_FXYID_MASK].CollisionLeft[y & 0xF];
                            if (maskHeight >= 0) {
                                maskHeight += tileX;
                                if (x >= maskHeight && M_ABS(x - maskHeight) <= 14) {
                                    sensor->Collided = true;
                                    sensor->Angle = TileCfg[entity360->PlaneIndex][*tile & TILE_FXYID_MASK].AngleLeft;
                                    sensor->X = (maskHeight + layer->CollideOffset.X.Whole) << 16;
                                    tileCheck = 3;
                                }
                            }
                        }

                        tileX += 16;
                        tile += 1;
                    }
                }

                x += layer->CollideOffset.X.Whole;
                y += layer->CollideOffset.Y.Whole;
            }

            layer++;
            layerBit <<= 1;
        }
    }
    void SensorHitCeiling(Sensor* sensor) {
        Uint16 layerBit = 1;
        Uint32 solidityMask;

        Layer* layer = Scene::Layers;

        int x = sensor->X >> 16;
        int y = sensor->Y >> 16;

        if (entity360->PlaneIndex == 0)
            solidityMask = ((TILE_COLLA_MASK >> 1) & TILE_COLLA_MASK) << 1; // 0x2000;
        else
            solidityMask = ((TILE_COLLB_MASK >> 1) & TILE_COLLB_MASK) << 1; // 0x8000;

        while (layerBit < 256) {
            if ((entity360->LayerCollisionFlag & layerBit)) {
                x -= layer->CollideOffset.X.Whole;
                y -= layer->CollideOffset.Y.Whole;

                int width = layer->Width << 4;
                int height = layer->Height << 4;
                int tileY = (y & 0xFFFFFFF0) + 16;
                if (x >= 0 && x < width) {
                    Tile* tile = &layer->Tiles[(x >> 4) | ((tileY >> 4) << layer->WidthInBits)];
                    for (int tileCheck = 0; tileCheck < 3; tileCheck++) {
                        if (tileY >= 0 && tileY < height && *tile != TILE_EMPTY && (*tile & solidityMask)) {
                            int maskHeight = TileCfg[entity360->PlaneIndex][*tile & TILE_FXYID_MASK].CollisionBottom[x & 0xF];
                            if (maskHeight >= 0) {
                                maskHeight += tileY;
                                if (y < maskHeight && M_ABS(y - maskHeight) <= 14) {
                                    sensor->Collided = true;
                                    sensor->Angle = TileCfg[entity360->PlaneIndex][*tile & TILE_FXYID_MASK].AngleBottom;
                                    sensor->Y = (maskHeight + layer->CollideOffset.Y.Whole) << 16;
                                    tileCheck = 3;
                                }
                            }
                        }

                        tileY -= 16;
                        tile -= 1 << layer->WidthInBits;
                    }
                }

                x += layer->CollideOffset.X.Whole;
                y += layer->CollideOffset.Y.Whole;
            }

            layer++;
            layerBit <<= 1;
        }
    }
    void SensorHitRightWall(Sensor* sensor) {
        Uint16 layerBit = 1;
        Uint32 solidityMask;

        Layer* layer = Scene::Layers;

        int x = sensor->X >> 16;
        int y = sensor->Y >> 16;

        if (entity360->PlaneIndex == 0)
            solidityMask = ((TILE_COLLA_MASK >> 1) & TILE_COLLA_MASK) << 1; // 0x2000;
        else
            solidityMask = ((TILE_COLLB_MASK >> 1) & TILE_COLLB_MASK) << 1; // 0x8000;

        while (layerBit < 256) {
            if ((entity360->LayerCollisionFlag & layerBit)) {
                x -= layer->CollideOffset.X.Whole;
                y -= layer->CollideOffset.Y.Whole;

                int width = layer->Width << 4;
                int height = layer->Height << 4;
                int tileX = (x & 0xFFFFFFF0) + 16;
                if (y >= 0 && y < height) {
                    Tile* tile = &layer->Tiles[(tileX >> 4) | ((y >> 4) << layer->WidthInBits)];
                    for (int tileCheck = 0; tileCheck < 3; tileCheck++) {
                        if (tileX >= 0 && tileX < width && *tile != TILE_EMPTY && (*tile & solidityMask)) {
                            int maskHeight = TileCfg[entity360->PlaneIndex][*tile & TILE_FXYID_MASK].CollisionRight[y & 0xF];
                            if (maskHeight >= 0) {
                                maskHeight += tileX;
                                if (x <= maskHeight && M_ABS(x - maskHeight) <= 14) {
                                    sensor->Collided = true;
                                    sensor->Angle = TileCfg[entity360->PlaneIndex][*tile & TILE_FXYID_MASK].AngleRight;
                                    sensor->X = (maskHeight + layer->CollideOffset.X.Whole) << 16;
                                    tileCheck = 3;
                                }
                            }
                        }

                        tileX -= 16;
                        tile -= 1;
                    }
                }

                x += layer->CollideOffset.X.Whole;
                y += layer->CollideOffset.Y.Whole;
            }

            layer++;
            layerBit <<= 1;
        }
    }
    void SensorGripFloor(Sensor* sensor) {
        Uint16 layerBit = 1;
        Uint32 solidityMask;

        Layer* layer = Scene::Layers;

        int x = sensor->X >> 16;
        int y = sensor->Y >> 16;
        int ycheck = y;

        if (entity360->PlaneIndex == 0)
            solidityMask = ((TILE_COLLA_MASK >> 1) & TILE_COLLA_MASK); // 0x1000;
        else
            solidityMask = ((TILE_COLLB_MASK >> 1) & TILE_COLLB_MASK); // 0x4000;

        while (layerBit < 256) {
            if ((entity360->LayerCollisionFlag & layerBit)) {
                x -= layer->CollideOffset.X.Whole;
                y -= layer->CollideOffset.Y.Whole;

                int width = layer->Width << 4;
                int height = layer->Height << 4;
                int tileY = (y & 0xFFFFFFF0) - 16;
                if (x >= 0 && x < width) {
                    Tile* tile = &layer->Tiles[(x >> 4) | ((tileY >> 4) << layer->WidthInBits)];
                    for (int tileCheck = 0; tileCheck < 3; tileCheck++) {
                        if (tileY >= 0 && tileY < height && *tile != TILE_EMPTY && (*tile & solidityMask)) {
                            int maskHeight = TileCfg[entity360->PlaneIndex][*tile & TILE_FXYID_MASK].CollisionTop[x & 0xF];
                            if (maskHeight >= 0) {
                                maskHeight += tileY;

                                if (!sensor->Collided || ycheck >= maskHeight) {
                                    int newAngle = TileCfg[entity360->PlaneIndex][*tile & TILE_FXYID_MASK].AngleTop;
                                    if (M_ABS(y - maskHeight) <= tolerance360 &&
                                        (sensor->Angle == ANGLE_OVERRIDE_CHECK || M_ABS(sensor->Angle - newAngle) <= 32 || M_ABS(sensor->Angle - newAngle + 256) <= 32 || M_ABS(sensor->Angle - newAngle - 256) <= 32)) {
                                        sensor->Collided = true;
                                        sensor->Angle = newAngle;
                                        sensor->Y = (maskHeight + layer->CollideOffset.Y.Whole) << 16;
                                        ycheck = maskHeight;
                                        tileCheck = 3;
                                    }
                                }
                            }
                        }

                        tileY += 16;
                        tile += 1 << layer->WidthInBits;
                    }
                }

                x += layer->CollideOffset.X.Whole;
                y += layer->CollideOffset.Y.Whole;
            }

            layer++;
            layerBit <<= 1;
        }
    }
    void SensorGripLeftWall(Sensor* sensor) {
        Uint16 layerBit = 1;
        Uint32 solidityMask;

        Layer* layer = Scene::Layers;

        int x = sensor->X >> 16;
        int y = sensor->Y >> 16;
        int xcheck = x;

        if (entity360->PlaneIndex == 0)
            solidityMask = TILE_COLLA_MASK;
        else
            solidityMask = TILE_COLLB_MASK;

        while (layerBit < 256) {
            if ((entity360->LayerCollisionFlag & layerBit)) {
                x -= layer->CollideOffset.X.Whole;
                y -= layer->CollideOffset.Y.Whole;

                int width = layer->Width << 4;
                int height = layer->Height << 4;
                int tileX = (x & 0xFFFFFFF0) - 16;
                if (y >= 0 && y < height) {
                    Tile* tile = &layer->Tiles[(tileX >> 4) | ((y >> 4) << layer->WidthInBits)];
                    for (int tileCheck = 0; tileCheck < 3; tileCheck++) {
                        if (tileX >= 0 && tileX < width && *tile != TILE_EMPTY && (*tile & solidityMask)) {
                            int maskHeight = TileCfg[entity360->PlaneIndex][*tile & TILE_FXYID_MASK].CollisionLeft[y & 0xF];
                            if (maskHeight >= 0) {
                                maskHeight += tileX;

                                if (!sensor->Collided || xcheck >= maskHeight) {
                                    int newAngle = TileCfg[entity360->PlaneIndex][*tile & TILE_FXYID_MASK].AngleLeft;
                                    if (M_ABS(x - maskHeight) <= tolerance360 &&
                                        (sensor->Angle == ANGLE_OVERRIDE_CHECK || M_ABS(sensor->Angle - newAngle) <= 32)) {
                                        sensor->Collided = true;
                                        sensor->Angle = newAngle;
                                        sensor->X = (maskHeight + layer->CollideOffset.X.Whole) << 16;
                                        xcheck = maskHeight;
                                        tileCheck = 3;
                                    }
                                }
                            }
                        }

                        tileX += 16;
                        tile += 1;
                    }
                }

                x += layer->CollideOffset.X.Whole;
                y += layer->CollideOffset.Y.Whole;
            }

            layer++;
            layerBit <<= 1;
        }
    }
    void SensorGripCeiling(Sensor* sensor) {
        Uint16 layerBit = 1;
        Uint32 solidityMask;

        Layer* layer = Scene::Layers;

        int x = sensor->X >> 16;
        int y = sensor->Y >> 16;
        int ycheck = y;

        if (entity360->PlaneIndex == 0)
            solidityMask = ((TILE_COLLA_MASK >> 1) & TILE_COLLA_MASK) << 1; // 0x2000;
        else
            solidityMask = ((TILE_COLLB_MASK >> 1) & TILE_COLLB_MASK) << 1; // 0x8000;

        while (layerBit < 256) {
            if ((entity360->LayerCollisionFlag & layerBit)) {
                x -= layer->CollideOffset.X.Whole;
                y -= layer->CollideOffset.Y.Whole;

                int width = layer->Width << 4;
                int height = layer->Height << 4;
                int tileY = (y & 0xFFFFFFF0) + 16;
                if (x >= 0 && x < width) {
                    Tile* tile = &layer->Tiles[(x >> 4) | ((tileY >> 4) << layer->WidthInBits)];
                    for (int tileCheck = 0; tileCheck < 3; tileCheck++) {
                        if (tileY >= 0 && tileY < height && *tile != TILE_EMPTY && (*tile & solidityMask)) {
                            int maskHeight = TileCfg[entity360->PlaneIndex][*tile & TILE_FXYID_MASK].CollisionBottom[x & 0xF];
                            if (maskHeight >= 0) {
                                maskHeight += tileY;

                                if (!sensor->Collided || ycheck <= maskHeight) {
                                    int newAngle = TileCfg[entity360->PlaneIndex][*tile & TILE_FXYID_MASK].AngleBottom;
                                    if (M_ABS(y - maskHeight) <= tolerance360 &&
                                        (sensor->Angle == ANGLE_OVERRIDE_CHECK || M_ABS(sensor->Angle - newAngle) <= 32)) {
                                        sensor->Collided = true;
                                        sensor->Angle = newAngle;
                                        sensor->Y = (maskHeight + layer->CollideOffset.Y.Whole) << 16;
                                        ycheck = maskHeight;
                                        tileCheck = 3;
                                    }
                                }
                            }
                        }

                        tileY -= 16;
                        tile -= 1 << layer->WidthInBits;
                    }
                }

                x += layer->CollideOffset.X.Whole;
                y += layer->CollideOffset.Y.Whole;
            }

            layer++;
            layerBit <<= 1;
        }
    }
    void SensorGripRightWall(Sensor* sensor) {
        Uint16 layerBit = 1;
        Uint32 solidityMask;

        Layer* layer = Scene::Layers;

        int x = sensor->X >> 16;
        int y = sensor->Y >> 16;
        int xcheck = x;

        if (entity360->PlaneIndex == 0)
            solidityMask = TILE_COLLA_MASK;
        else
            solidityMask = TILE_COLLB_MASK;

        while (layerBit < 256) {
            if ((entity360->LayerCollisionFlag & layerBit)) {
                x -= layer->CollideOffset.X.Whole;
                y -= layer->CollideOffset.Y.Whole;

                int width = layer->Width << 4;
                int height = layer->Height << 4;
                int tileX = (x & 0xFFFFFFF0) + 16;
                if (y >= 0 && y < height) {
                    Tile* tile = &layer->Tiles[(tileX >> 4) | ((y >> 4) << layer->WidthInBits)];
                    for (int tileCheck = 0; tileCheck < 3; tileCheck++) {
                        if (tileX >= 0 && tileX < width && *tile != TILE_EMPTY && (*tile & solidityMask)) {
                            int maskHeight = TileCfg[entity360->PlaneIndex][*tile & TILE_FXYID_MASK].CollisionRight[y & 0xF];
                            if (maskHeight >= 0) {
                                maskHeight += tileX;

                                if (!sensor->Collided || xcheck <= maskHeight) {
                                    int newAngle = TileCfg[entity360->PlaneIndex][*tile & TILE_FXYID_MASK].AngleRight;
                                    if (M_ABS(x - maskHeight) <= tolerance360 &&
                                        (sensor->Angle == ANGLE_OVERRIDE_CHECK || M_ABS(sensor->Angle - newAngle) <= 32)) {
                                        sensor->Collided = true;
                                        sensor->Angle = newAngle;
                                        sensor->X = (maskHeight + layer->CollideOffset.X.Whole) << 16;
                                        xcheck = maskHeight;
                                        tileCheck = 3;
                                    }
                                }
                            }
                        }

                        tileX -= 16;
                        tile -= 1;
                    }
                }

                x += layer->CollideOffset.X.Whole;
                y += layer->CollideOffset.Y.Whole;
            }

            layer++;
            layerBit <<= 1;
        }
    }
    void C360Hit() {
        Sensor sensors[6];
        int deltaX;
        int deltaY;
        int loopSize;
        int loopRemainderX;
        int loopRemainderY;
        int sLoop;

        // 0: none, 1: check tile
        int modeUp = 0;
        int modeDown = 0;
        int modeLeft = 0;
        int modeRight = 0;

        int xspeed = entity360->Speed.X.Full;
        if (xspeed >= 0) {
            modeRight = 1;

            sensors[0].X = entity360->Position.X.Full + (outerHitbox360.Right << 16);
            sensors[0].Y = entity360->Position.Y.Full + yoffset360;
            sensors[0].Collided = false;
        }

        if (xspeed <= 0) {
            modeLeft = 1;

            sensors[1].X = entity360->Position.X.Full + (outerHitbox360.Left << 16) - 0x10000;
            sensors[1].Y = entity360->Position.Y.Full + yoffset360;
            sensors[1].Collided = false;
        }

        sensors[2].X = entity360->Position.X.Full + (outerHitbox360.Left << 16) + 0x10000;
        sensors[3].X = entity360->Position.X.Full + (outerHitbox360.Right << 16) - 0x20000;
        sensors[2].Collided = false;
        sensors[3].Collided = false;

        sensors[4].X = sensors[2].X;
        sensors[5].X = sensors[3].X;
        sensors[4].Collided = false;
        sensors[5].Collided = false;

        if (entity360->Speed.Y.Full >= 0) {
            modeDown = 1;
            sensors[2].Y =
            sensors[3].Y = entity360->Position.Y.Full + (outerHitbox360.Bottom << 16);
        }

        if (M_ABS(entity360->Speed.X.Full) > 0x10000 || entity360->Speed.Y.Full < 0) {
            modeUp = 1;
            sensors[4].Y =
            sensors[5].Y = entity360->Position.Y.Full + (outerHitbox360.Top << 16) - 0x10000;
        }

        if (M_ABS(entity360->Speed.X.Full) > M_ABS(entity360->Speed.Y.Full))
            loopSize = (M_ABS(entity360->Speed.X.Full) >> granularity360) + 1;
        else
            loopSize = (M_ABS(entity360->Speed.Y.Full) >> granularity360) + 1;

        deltaX = entity360->Speed.X.Full / loopSize;
        deltaY = entity360->Speed.Y.Full / loopSize;
        loopRemainderX = entity360->Speed.X.Full - (deltaX * (loopSize - 1));
        loopRemainderY = entity360->Speed.Y.Full - (deltaY * (loopSize - 1));

        while (loopSize > 0) {
            if (loopSize < 2) {
                deltaX = loopRemainderX;
                deltaY = loopRemainderY;
            }
            loopSize--;

            if (modeRight == 1) {
                sensors[0].X += deltaX;
                sensors[0].Y += deltaY;
                SensorHitLeftWall(&sensors[0]);

                if (sensors[0].Collided) {
                    modeRight = 2;
                }
                else if (entity360->Speed.X.Full < 0x20000 && yoffset360 > 0) {
                    sensors[0].Y -= yoffset360 << 1;
                    SensorHitLeftWall(&sensors[0]);

                    if (sensors[0].Collided)
                        modeRight = 2;

                    sensors[0].Y += yoffset360 << 1;
                }
            }

            if (modeLeft == 1) {
                sensors[1].X += deltaX;
                sensors[1].Y += deltaY;
                SensorHitRightWall(&sensors[1]);

                if (sensors[1].Collided) {
                    modeLeft = 2;
                }
                else if (entity360->Speed.X.Full > -0x20000 && yoffset360 > 0) {
                    sensors[1].Y -= yoffset360 << 1;
                    SensorHitRightWall(&sensors[1]);

                    if (sensors[1].Collided)
                        modeLeft = 2;

                    sensors[1].Y += yoffset360 << 1;
                }
            }

            if (modeRight == 2) {
                entity360->Speed.X.Full = 0;
                entity360->GroundSpeed.Full = 0;
                entity360->Position.X.Full = sensors[0].X - (outerHitbox360.Right << 16);

                sensors[2].X = sensors[4].X = entity360->Position.X.Full + (outerHitbox360.Left << 16) + 0x10000;
                sensors[3].X = sensors[5].X = entity360->Position.X.Full + (outerHitbox360.Right << 16) - 0x20000;

                deltaX = 0;
                loopRemainderX = 0;

                modeRight = 3;
            }

            if (modeLeft == 2) {
                entity360->Speed.X.Full = 0;
                entity360->GroundSpeed.Full = 0;
                entity360->Position.X.Full = sensors[1].X - (outerHitbox360.Left << 16) + 0x10000;

                sensors[2].X = sensors[4].X = entity360->Position.X.Full + (outerHitbox360.Left << 16) + 0x10000;
                sensors[3].X = sensors[5].X = entity360->Position.X.Full + (outerHitbox360.Right << 16) - 0x20000;

                deltaX = 0;
                loopRemainderX = 0;

                modeLeft = 3;
            }

            if (modeDown == 1) {
                for (sLoop = 2; sLoop < 4; sLoop++) {
                    if (!sensors[sLoop].Collided) {
                        sensors[sLoop].X += deltaX;
                        sensors[sLoop].Y += deltaY;
                        SensorHitFloor(&sensors[sLoop]);
                    }
                }
                if (sensors[2].Collided || sensors[3].Collided) {
                    modeDown = 2;
                    loopSize = 0;
                }
            }

            if (modeUp == 1) {
                for (sLoop = 4; sLoop < 6; sLoop++) {
                    if (!sensors[sLoop].Collided) {
                        sensors[sLoop].X += deltaX;
                        sensors[sLoop].Y += deltaY;
                        SensorHitCeiling(&sensors[sLoop]);
                    }
                }
                if (sensors[4].Collided || sensors[5].Collided) {
                    modeUp = 2;
                    loopSize = 0;
                }
            }
        }

        if (modeRight < 2 && modeLeft < 2)
            entity360->Position.X.Full += entity360->Speed.X.Full;

        if (modeUp < 2 && modeDown < 2) {
            entity360->Position.Y.Full += entity360->Speed.Y.Full;
        }
        else {
            if (modeDown == 2) {
                entity360->Grounded = true;

                if (sensors[2].Collided && sensors[3].Collided) {
                    if (sensors[2].Y < sensors[3].Y) {
                        entity360->Position.Y.Full = sensors[2].Y - (outerHitbox360.Bottom << 16);
                        entity360->Angle = sensors[2].Angle;
                    }
                    else {
                        entity360->Position.Y.Full = sensors[3].Y - (outerHitbox360.Bottom << 16);
                        entity360->Angle = sensors[3].Angle;
                    }
                }
                else {
                    if (sensors[2].Collided) {
                        entity360->Position.Y.Full = sensors[2].Y - (outerHitbox360.Bottom << 16);
                        entity360->Angle = sensors[2].Angle;
                    }
                    else if (sensors[3].Collided) {
                        entity360->Position.Y.Full = sensors[3].Y - (outerHitbox360.Bottom << 16);
                        entity360->Angle = sensors[3].Angle;
                    }
                }

                if (entity360->Angle > 160 && entity360->Angle < 222 && entity360->AngleMode != 1) {
                    entity360->AngleMode = 1;
                    entity360->Position.X.Full -= 0x40000;
                }

                if (entity360->Angle > 34 && entity360->Angle < 96 && entity360->AngleMode != 3) {
                    entity360->AngleMode = 3;
                    entity360->Position.X.Full += 0x40000;
                }

                if (entity360->Angle < 128) {
                    if (entity360->Angle < 16) {
                        sLoop = entity360->Speed.X.Full;
                    }
                    else {
                        if (entity360->Angle < 32) {
                            if (M_ABS(entity360->Speed.X.Full) > M_ABS(entity360->Speed.Y.Full))
                                sLoop = entity360->Speed.X.Full;
                            else
                                sLoop = entity360->Speed.Y.Full >> 1;
                        }
                        else {
                            if (M_ABS(entity360->Speed.X.Full) > M_ABS(entity360->Speed.Y.Full))
                                sLoop = entity360->Speed.X.Full;
                            else
                                sLoop = entity360->Speed.Y.Full;
                        }
                    }
                }
                else {
                    if (entity360->Angle > 240) {
                        sLoop = entity360->Speed.X.Full;
                    }
                    else {
                        if (entity360->Angle > 224) {
                            if (M_ABS(entity360->Speed.X.Full) > M_ABS(entity360->Speed.Y.Full))
                                sLoop = entity360->Speed.X.Full;
                            else
                                sLoop = -(entity360->Speed.Y.Full >> 1);
                        }
                        else {
                            if (M_ABS(entity360->Speed.X.Full) > M_ABS(entity360->Speed.Y.Full))
                                sLoop = entity360->Speed.X.Full;
                            else
                                sLoop = -entity360->Speed.Y.Full;
                        }
                    }
                }

                if (sLoop < -0x180000)
                    sLoop = -0x180000;
                if (sLoop >  0x180000)
                    sLoop =  0x180000;

                entity360->GroundSpeed.Full = sLoop;
                entity360->Speed.X.Full = entity360->GroundSpeed.Full;
                entity360->Speed.Y.Full = 0;
            }

            if (modeUp == 2) {
                if (sensors[4].Collided && sensors[5].Collided) {
                    if (sensors[4].Y > sensors[5].Y) {
                        entity360->Position.Y.Full = sensors[4].Y - (outerHitbox360.Top << 16) + 0x10000;
                        loopSize = sensors[4].Angle;
                    }
                    else {
                        entity360->Position.Y.Full = sensors[5].Y - (outerHitbox360.Top << 16) + 0x10000;
                        loopSize = sensors[5].Angle;
                    }
                }
                else {
                    if (sensors[4].Collided) {
                        entity360->Position.Y.Full = sensors[4].Y - (outerHitbox360.Top << 16) + 0x10000;
                        loopSize = sensors[4].Angle;
                    }
                    else if (sensors[5].Collided) {
                        entity360->Position.Y.Full = sensors[5].Y - (outerHitbox360.Top << 16) + 0x10000;
                        loopSize = sensors[5].Angle;
                    }
                }

                loopSize &= 0xFF;

                if (loopSize > 63 && loopSize < 98) {
                    if (entity360->Speed.Y.Full < -M_ABS(entity360->Speed.X.Full)) {
                        entity360->Grounded = true;
                        entity360->Angle = loopSize;

                        entity360->AngleMode = 3;
                        entity360->Position.X.Full += 0x40000;
                        entity360->Position.Y.Full -= 0x20000;

                        if (entity360->Angle > 96)
                            entity360->GroundSpeed.Full = entity360->Speed.Y.Full >> 1;
                        else
                            entity360->GroundSpeed.Full = entity360->Speed.Y.Full;
                    }
                }
                else if (loopSize > 158 && loopSize < 193) {
                    if (entity360->Speed.Y.Full < -M_ABS(entity360->Speed.X.Full)) {
                        entity360->Grounded = true;
                        entity360->Angle = loopSize;

                        entity360->AngleMode = 1;
                        entity360->Position.X.Full -= 0x40000;
                        entity360->Position.Y.Full -= 0x20000;

                        if (entity360->Angle < 160)
                            entity360->GroundSpeed.Full = -(entity360->Speed.Y.Full >> 1);
                        else
                            entity360->GroundSpeed.Full = -entity360->Speed.Y.Full;
                    }
                }

                if (entity360->Speed.Y.Full < 0)
                    entity360->Speed.Y.Full = 0;
            }
        }
    }
    void C360Grip() {
        Sensor sensors[5];
        int deltaX;
        int deltaY;
        int loopSize;
        int loopRemainder;
        int sFlag = -1;
        int sLoop;

        sensors[4].X = entity360->Position.X.Full;
        sensors[4].Y = entity360->Position.Y.Full;

        sensors[0].Angle =
        sensors[1].Angle =
        sensors[2].Angle = entity360->Angle;

        sensors[0].Collided =
        sensors[1].Collided =
        sensors[2].Collided =
        sensors[3].Collided = false;

        SetGripSensors(sensors);

        loopRemainder = M_ABS(entity360->GroundSpeed.Full);
        loopSize = loopRemainder >> 18;
        loopRemainder &= 0x3FFFF;

        while (loopSize > -1) {
            if (loopSize < 1) {
                deltaX = (Math::CosTbl_0x100[entity360->Angle] * loopRemainder) >> 8;
                deltaY = (Math::SinTbl_0x100[entity360->Angle] * loopRemainder) >> 8;
                loopSize = -1;
            }
            else {
                deltaX = Math::CosTbl_0x100[entity360->Angle] << 10; // to 18
                deltaY = Math::SinTbl_0x100[entity360->Angle] << 10;
                loopSize--;
            }

            if (entity360->GroundSpeed.Full < 0) {
                deltaX = -deltaX;
                deltaY = -deltaY;
            }

            sensors[0].Collided =
            sensors[1].Collided =
            sensors[2].Collided =
            sensors[3].Collided = false;

            sensors[4].X += deltaX;
            sensors[4].Y += deltaY;

            switch (entity360->AngleMode) {
                case 0:
                    sensors[3].X += deltaX;
                    sensors[3].Y += deltaY;
                    if (entity360->GroundSpeed.Full > 0) {
                        SensorHitLeftWall(&sensors[3]);

                        if (sensors[3].Collided) {
                            deltaX = 0;
                            loopSize = -1;
                            sensors[2].X = sensors[3].X - 0x20000;
                        }
                    }
                    else if (entity360->GroundSpeed.Full < 0) {
                        SensorHitRightWall(&sensors[3]);

                        if (sensors[3].Collided) {
                            deltaX = 0;
                            loopSize = -1;
                            sensors[0].X = sensors[3].X + 0x20000;
                        }
                    }

                    for (sLoop = 0; sLoop < 3; sLoop++) {
                        sensors[sLoop].X += deltaX;
                        sensors[sLoop].Y += deltaY;
                        SensorGripFloor(&sensors[sLoop]);
                    }

                    sFlag = -1;
                    for (sLoop = 0; sLoop < 3; sLoop++) {
                        if (sFlag > -1) {
                            if (sensors[sLoop].Collided) {
                                if (sensors[sLoop].Y < sensors[sFlag].Y)
                                    sFlag = sLoop;
                                else if (sensors[sLoop].Y == sensors[sFlag].Y &&
                                    (sensors[sLoop].Angle < 0x08 || sensors[sLoop].Angle > 0xF8))
                                    sFlag = sLoop;
                            }
                        }
                        else if (sensors[sLoop].Collided)
                            sFlag = sLoop;
                    }

                    if (sFlag > -1) {
                        sensors[0].Y =
                        sensors[1].Y =
                        sensors[2].Y = sensors[sFlag].Y;
                        sensors[0].Angle =
                        sensors[1].Angle =
                        sensors[2].Angle = sensors[sFlag].Angle;

                        sensors[4].X = sensors[1].X;
                        sensors[4].Y = sensors[1].Y - (outerHitbox360.Bottom << 16);
                    }
                    else {
                        loopSize = -1;
                    }

                    if (sensors[0].Angle < 222 && sensors[0].Angle > 128)
                        entity360->AngleMode = 1;
                    if (sensors[0].Angle > 34 && sensors[0].Angle < 128)
                        entity360->AngleMode = 3;
                    break;
                case 1:
                    sensors[3].X += deltaX;
                    sensors[3].Y += deltaY;
                    if (entity360->GroundSpeed.Full > 0)
                        SensorHitCeiling(&sensors[3]);
                    if (entity360->GroundSpeed.Full < 0)
                        SensorHitFloor(&sensors[3]);

                    if (sensors[3].Collided) {
                        deltaY = 0;
                        loopSize = -1;
                    }

                    for (sLoop = 0; sLoop < 3; sLoop++) {
                        sensors[sLoop].X += deltaX;
                        sensors[sLoop].Y += deltaY;
                        SensorGripLeftWall(&sensors[sLoop]);
                    }

                    sFlag = -1;
                    for (sLoop = 0; sLoop < 3; sLoop++) {
                        if (sFlag > -1) {
                            if (sensors[sLoop].Collided) {
                                if (sensors[sLoop].X < sensors[sFlag].X)
                                    sFlag = sLoop;
                            }
                        }
                        else if (sensors[sLoop].Collided)
                            sFlag = sLoop;
                    }

                    if (sFlag > -1) {
                        sensors[0].X =
                        sensors[1].X =
                        sensors[2].X = sensors[sFlag].X;
                        sensors[0].Angle =
                        sensors[1].Angle =
                        sensors[2].Angle = sensors[sFlag].Angle;

                        sensors[4].Y = sensors[1].Y;
                        sensors[4].X = sensors[1].X - (outerHitbox360.Bottom << 16);
                    }
                    else {
                        loopSize = -1;
                    }

                    if (sensors[0].Angle > 226)
                        entity360->AngleMode = 0;
                    if (sensors[0].Angle < 158)
                        entity360->AngleMode = 2;
                    break;
                case 2:
                    sensors[3].X += deltaX;
                    sensors[3].Y += deltaY;
                    if (entity360->GroundSpeed.Full > 0)
                        SensorHitRightWall(&sensors[3]);

                    if (entity360->GroundSpeed.Full < 0)
                        SensorHitLeftWall(&sensors[3]);

                    if (sensors[3].Collided) {
                        deltaX = 0;
                        loopSize = -1;
                    }

                    for (sLoop = 0; sLoop < 3; sLoop++) {
                        sensors[sLoop].X += deltaX;
                        sensors[sLoop].Y += deltaY;
                        SensorGripCeiling(&sensors[sLoop]);
                    }

                    sFlag = -1;
                    for (sLoop = 0; sLoop < 3; sLoop++) {
                        if (sFlag > -1) {
                            if (sensors[sLoop].Collided) {
                                if (sensors[sLoop].Y > sensors[sFlag].Y)
                                    sFlag = sLoop;
                            }
                        }
                        else if (sensors[sLoop].Collided)
                            sFlag = sLoop;
                    }

                    if (sFlag > -1) {
                        sensors[0].Y =
                        sensors[1].Y =
                        sensors[2].Y = sensors[sFlag].Y;
                        sensors[0].Angle =
                        sensors[1].Angle =
                        sensors[2].Angle = sensors[sFlag].Angle;

                        sensors[4].X = sensors[1].X;
                        sensors[4].Y = sensors[1].Y + (outerHitbox360.Bottom << 16) + 0x10000;
                    }
                    else {
                        loopSize = -1;
                    }

                    if (sensors[0].Angle > 162)
                        entity360->AngleMode = 1;
                    if (sensors[0].Angle < 94)
                        entity360->AngleMode = 3;
                    break;
                case 3:
                    sensors[3].X += deltaX;
                    sensors[3].Y += deltaY;
                    if (entity360->GroundSpeed.Full > 0)
                        SensorHitFloor(&sensors[3]);
                    if (entity360->GroundSpeed.Full < 0)
                        SensorHitCeiling(&sensors[3]);

                    if (sensors[3].Collided) {
                        deltaY = 0;
                        loopSize = -1;
                    }

                    for (sLoop = 0; sLoop < 3; sLoop++) {
                        sensors[sLoop].X += deltaX;
                        sensors[sLoop].Y += deltaY;
                        SensorGripRightWall(&sensors[sLoop]);
                    }

                    sFlag = -1;
                    for (sLoop = 0; sLoop < 3; sLoop++) {
                        if (sFlag > -1) {
                            if (sensors[sLoop].Collided) {
                                if (sensors[sLoop].X > sensors[sFlag].X)
                                    sFlag = sLoop;
                            }
                        }
                        else if (sensors[sLoop].Collided)
                            sFlag = sLoop;
                    }

                    if (sFlag > -1) {
                        sensors[0].X =
                        sensors[1].X =
                        sensors[2].X = sensors[sFlag].X;
                        sensors[0].Angle =
                        sensors[1].Angle =
                        sensors[2].Angle = sensors[sFlag].Angle;

                        sensors[4].Y = sensors[1].Y;
                        sensors[4].X = sensors[1].X + (outerHitbox360.Bottom << 16) + 0x10000;
                    }
                    else {
                        loopSize = -1;
                    }

                    if (sensors[0].Angle < 30)
                        entity360->AngleMode = 0;
                    if (sensors[0].Angle > 98)
                        entity360->AngleMode = 2;
                    break;
            }

            if (sensors[3].Collided) {
                loopSize = -2;

                if (sFlag > -1)
                    entity360->Angle = sensors[0].Angle;
            }
            else if (sFlag > -1) {
                entity360->Angle = sensors[0].Angle;
                SetGripSensors(sensors);
            }

            switch (entity360->AngleMode) {
                case 0:
                    if (!sensors[0].Collided && !sensors[1].Collided && !sensors[2].Collided) {
                        entity360->Grounded = false;
                        entity360->AngleMode = 0;
                        entity360->Speed.X.Full = (Math::CosTbl_0x100[entity360->Angle] * entity360->GroundSpeed.Full) >> 8;
                        entity360->Speed.Y.Full = (Math::SinTbl_0x100[entity360->Angle] * entity360->GroundSpeed.Full) >> 8;

                        if (entity360->Speed.Y.Full < -0x100000)
                            entity360->Speed.Y.Full = -0x100000;
                        else if (entity360->Speed.Y.Full > 0x100000)
                            entity360->Speed.Y.Full = 0x100000;

                        entity360->GroundSpeed = entity360->Speed.X;
                        entity360->Angle = 0;

                        if (sensors[3].Collided) {
                            if (entity360->GroundSpeed.Full > 0)
                                entity360->Position.X.Full = sensors[3].X - (outerHitbox360.Right << 16);
                            else if (entity360->GroundSpeed.Full < 0)
                                entity360->Position.X.Full = sensors[3].X - (outerHitbox360.Left << 16) + 0x10000;

                            entity360->Speed.X.Full = 0;
                            entity360->GroundSpeed.Full = 0;
                        }
                        else {
                            entity360->Position.X.Full += entity360->Speed.X.Full;
                        }
                        entity360->Position.Y.Full += entity360->Speed.Y.Full;
                    }
                    else {
                        entity360->Angle = sensors[0].Angle;

                        if (sensors[3].Collided) {
                            if (entity360->GroundSpeed.Full > 0)
                                entity360->Position.X.Full = sensors[3].X - (outerHitbox360.Right << 16);
                            else if (entity360->GroundSpeed.Full < 0)
                                entity360->Position.X.Full = sensors[3].X - (outerHitbox360.Left << 16) + 0x10000;

                            entity360->Speed.X.Full = 0;
                            entity360->GroundSpeed.Full = 0;
                        }
                        else {
                            entity360->Position.X.Full = sensors[4].X;
                        }

                        entity360->Position.Y.Full = sensors[4].Y;
                    }
                    break;
                case 1:
                    if (!sensors[0].Collided && !sensors[1].Collided && !sensors[2].Collided) {
                        entity360->Grounded = false;
                        entity360->AngleMode = 0;
                        entity360->Speed.X.Full = (Math::CosTbl_0x100[entity360->Angle] * entity360->GroundSpeed.Full) >> 8;
                        entity360->Speed.Y.Full = (Math::SinTbl_0x100[entity360->Angle] * entity360->GroundSpeed.Full) >> 8;

                        if (entity360->Speed.Y.Full < -0x100000)
                            entity360->Speed.Y.Full = -0x100000;
                        else if (entity360->Speed.Y.Full > 0x100000)
                            entity360->Speed.Y.Full = 0x100000;

                        entity360->GroundSpeed = entity360->Speed.X;
                        entity360->Angle = 0;
                    }
                    else {
                        entity360->Angle = sensors[0].Angle;
                    }

                    if (sensors[3].Collided) {
                        if (entity360->GroundSpeed.Full > 0)
                            entity360->Position.Y.Full = sensors[3].Y + (outerHitbox360.Right << 16) + 0x10000;
                        else if (entity360->GroundSpeed.Full < 0)
                            entity360->Position.Y.Full = sensors[3].Y - (outerHitbox360.Left << 16);

                        entity360->GroundSpeed.Full = 0;
                    }
                    else {
                        entity360->Position.Y.Full = sensors[4].Y;
                    }

                    entity360->Position.X.Full = sensors[4].X;
                    break;
                case 2:
                    if (!sensors[0].Collided && !sensors[1].Collided && !sensors[2].Collided) {
                        entity360->Grounded = false;
                        entity360->AngleMode = 0;
                        entity360->Speed.X.Full = (Math::CosTbl_0x100[entity360->Angle] * entity360->GroundSpeed.Full) >> 8;
                        entity360->Speed.Y.Full = (Math::SinTbl_0x100[entity360->Angle] * entity360->GroundSpeed.Full) >> 8;

                        if (entity360->Speed.Y.Full < -0x100000)
                            entity360->Speed.Y.Full = -0x100000;
                        else if (entity360->Speed.Y.Full > 0x100000)
                            entity360->Speed.Y.Full = 0x100000;

                        entity360->GroundSpeed = entity360->Speed.X;
                        entity360->Angle = 0;

                        if (sensors[3].Collided) {
                            if (entity360->GroundSpeed.Full > 0)
                                entity360->Position.X.Full = sensors[3].X - (outerHitbox360.Right << 16);
                            else if (entity360->GroundSpeed.Full < 0)
                                entity360->Position.X.Full = sensors[3].X - (outerHitbox360.Left << 16) + 0x10000;

                            entity360->GroundSpeed.Full = 0;
                        }
                        else {
                            entity360->Position.X.Full += entity360->Speed.X.Full;
                        }
                    }
                    else {
                        entity360->Angle = sensors[0].Angle;

                        if (sensors[3].Collided) {
                            if (entity360->GroundSpeed.Full > 0)
                                entity360->Position.X.Full = sensors[3].X + (outerHitbox360.Right << 16);
                            else if (entity360->GroundSpeed.Full < 0)
                                entity360->Position.X.Full = sensors[3].X + (outerHitbox360.Left << 16) - 0x10000;

                            entity360->GroundSpeed.Full = 0;
                        }
                        else {
                            entity360->Position.X.Full = sensors[4].X;
                        }
                    }
                    entity360->Position.Y.Full = sensors[4].Y;
                    break;
                case 3:
                    if (!sensors[0].Collided && !sensors[1].Collided && !sensors[2].Collided) {
                        entity360->Grounded = false;
                        entity360->AngleMode = 0;
                        entity360->Speed.X.Full = (Math::CosTbl_0x100[entity360->Angle] * entity360->GroundSpeed.Full) >> 8;
                        entity360->Speed.Y.Full = (Math::SinTbl_0x100[entity360->Angle] * entity360->GroundSpeed.Full) >> 8;

                        if (entity360->Speed.Y.Full < -0x100000)
                            entity360->Speed.Y.Full = -0x100000;
                        else if (entity360->Speed.Y.Full > 0x100000)
                            entity360->Speed.Y.Full = 0x100000;

                        entity360->GroundSpeed = entity360->Speed.X;
                        entity360->Angle = 0;
                    }
                    else {
                        entity360->Angle = sensors[0].Angle;
                    }

                    if (sensors[3].Collided) {
                        if (entity360->GroundSpeed.Full > 0)
                            entity360->Position.Y.Full = sensors[3].Y - (outerHitbox360.Right << 16);
                        else if (entity360->GroundSpeed.Full < 0)
                            entity360->Position.Y.Full = sensors[3].Y - (outerHitbox360.Left << 16) + 0x10000;

                        entity360->GroundSpeed.Full = 0;
                    }
                    else {
                        entity360->Position.Y.Full = sensors[4].Y;
                    }

                    entity360->Position.X.Full = sensors[4].X;
                    break;
            }
        }
    }
    void C360Movement(Entity* entity, Hitbox* outerHitbox, Hitbox* innerHitbox) {
        if (!entity || !outerHitbox || !innerHitbox)
            return;

        if (!entity->TileCollision) {
            entity->Position.X.Full += entity->Speed.X.Full;
            entity->Position.Y.Full += entity->Speed.Y.Full;
            return;
        }

        entity->Angle &= 0xFF;

        if (M_ABS(entity->GroundSpeed.Full) < 0x60000 && entity->Angle == 0)
            tolerance360 = 8;
        else
            tolerance360 = 15;

        entity360 = entity;
        memcpy(&outerHitbox360, outerHitbox, sizeof(Hitbox));
        memcpy(&innerHitbox360, innerHitbox, sizeof(Hitbox));

        if (outerHitbox360.Bottom < 14) {
            yoffset360 = 0;
            tolerance360 = 15;
            granularity360 = 17;
        }
        else {
            yoffset360 = 0x40000;
            granularity360 = 19;
        }

        if (!entity->Grounded)
            C360Hit();
        else
            C360Grip();

        if (entity->Grounded) {
            entity->Speed.X.Full = (Math::CosTbl_0x100[entity->Angle] * entity->GroundSpeed.Full) >> 8;
            entity->Speed.Y.Full = (Math::SinTbl_0x100[entity->Angle] * entity->GroundSpeed.Full) >> 8;
        }
        else {
            entity->GroundSpeed.Full = entity->Speed.X.Full;
        }
    }

    Entity tileDetectEntity;

    bool TileHit(Vector2* position, Uint16 layerCollisionFlag, Uint8 angleMode, Uint8 planeIndex, Subpixels offsetX, Subpixels offsetY, bool grip) {
        auto old_entity360 = entity360;
        auto old_tolerance360 = tolerance360;

        bool collided = false;

        entity360 = &tileDetectEntity;
        entity360->PlaneIndex = planeIndex;
        entity360->LayerCollisionFlag = layerCollisionFlag;

        Sensor sensor;
        sensor.Collided = false;
        sensor.X = position->X + offsetX;
        sensor.Y = position->Y + offsetY;
        sensor.Angle = ANGLE_OVERRIDE_CHECK;

        switch (angleMode) {
        case 0:
            SensorHitFloor(&sensor);
            if (sensor.Collided) {
                if (grip)
                    position->Y = sensor.Y - offsetY;

                collided = true;
            }
            break;
        case 1:
            SensorHitLeftWall(&sensor);
            if (sensor.Collided) {
                if (grip)
                    position->X = sensor.X - offsetX;

                collided = true;
            }
            break;
        case 2:
            SensorHitCeiling(&sensor);
            if (sensor.Collided) {
                if (grip)
                    position->Y = sensor.Y - offsetY;

                collided = true;
            }
            break;
        case 3:
            SensorHitRightWall(&sensor);
            if (sensor.Collided) {
                if (grip)
                    position->X = sensor.X - offsetX;

                collided = true;
            }
            break;
        }

        tolerance360 = old_tolerance360;
        entity360 = old_entity360;
        return collided;
    }
    bool TileGrip(Vector2* position, Uint16 layerCollisionFlag, Uint8 angleMode, Uint8 planeIndex, Subpixels offsetX, Subpixels offsetY, int tolerance) {
        auto old_entity360 = entity360;
        auto old_tolerance360 = tolerance360;

        bool collided = false;

        entity360 = &tileDetectEntity;
        entity360->PlaneIndex = planeIndex;
        entity360->LayerCollisionFlag = layerCollisionFlag;

        tolerance360 = tolerance;

        Sensor sensor;
        sensor.Collided = false;
        sensor.X = position->X + offsetX;
        sensor.Y = position->Y + offsetY;
        sensor.Angle = ANGLE_OVERRIDE_CHECK;

        switch (angleMode) {
        case 0:
            SensorGripFloor(&sensor);
            if (sensor.Collided) {
                position->Y = sensor.Y - offsetY;
                collided = true;
            }
            break;
        case 1:
            SensorGripLeftWall(&sensor);
            if (sensor.Collided) {
                position->X = sensor.X - offsetX;
                collided = true;
            }
            break;
        case 2:
            SensorGripCeiling(&sensor);
            if (sensor.Collided) {
                position->Y = sensor.Y - offsetY;
                collided = true;
            }
            break;
        case 3:
            SensorGripRightWall(&sensor);
            if (sensor.Collided) {
                position->X = sensor.X - offsetX;
                collided = true;
            }
            break;
        }

        tolerance360 = old_tolerance360;
        entity360 = old_entity360;
        return collided;
    }
}
