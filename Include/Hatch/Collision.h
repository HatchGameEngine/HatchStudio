#pragma once

namespace Collision {
    extern TileConfig TileCfg[2][MAX_TILE_COUNT << 2];

    bool EntitiesAABB(Entity* entityA, Hitbox* hitboxA, Entity* entityB, Hitbox* hitboxB);
    bool EntitiesCircular(Entity* entityA, Subpixels radiusA, Entity* entityB, Subpixels radiusB);
    int  EntitiesSolid(Entity* entitySolid, Hitbox* hitboxSolid, Entity* entity, Hitbox* hitbox, bool adjustSpeeds);
    bool EntitiesPlatform(Entity* entityPlatform, Hitbox* hitboxPlatform, Entity* entity, Hitbox* hitbox, bool adjustSpeeds);
    void C360Movement(Entity* entity, Hitbox* outerHitbox, Hitbox* innerHitbox);

    bool TileHit(Vector2* position, Uint16 layerCollisionFlag, Uint8 angleMode, Uint8 planeIndex, Subpixels offsetX, Subpixels offsetY, bool grip);
    bool TileGrip(Vector2* position, Uint16 layerCollisionFlag, Uint8 angleMode, Uint8 planeIndex, Subpixels offsetX, Subpixels offsetY, int tolerance);
}
