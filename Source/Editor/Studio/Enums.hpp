#pragma once

// Networking: ASIO that builds a queue of messages, read after event polling
enum class PacketTypes {
    Error,
    Join,
    TransferCommand,
};
namespace CommandIDs {
    enum {
        Error,
        LayerTileEditCommand,
        LayerTileSelectionEditCommand,
    };
};

enum EditorTypes {
    SCENE,
    SPRITE,
    TILECONFIG
};

enum class ResourceFileType {
    Unknown,

    Scene_RSDKv5,
    Scene_Tiled,
    Scene_HatchLite,

    TileCol_RSDKv5,
    TileCol_Hatch
};
