#pragma once

struct $safeitemname$ : Entity {
    // Static Object for $safeitemname$
    struct StaticObject : ::StaticObject {};
    DEFINE_STATIC;

    // Enums
    // ==============================================

    // enum {};

    // Entity variables
    // ==============================================

    Status status;

    // Events
    // ==============================================

    // Occurs when the scene loads
    static void OnStageLoad();
    // Occurs when the scene loads (in editor)
    static void OnEditorLoad();
    // Occurs when the entity is created, either after a scene loads,
    // or after is created during the game.
    void OnCreate(CreateFlag flag);
    // Occurs once every frame, before regular entities OnUpdate.
    static void OnStaticUpdate();
    // Occurs once every frame for every entity.
    void OnUpdate();
    // Occurs once every frame for every entity, after all OnUpdates
    // have been run.
    void OnUpdateLate();
    // Occurs when it is time to draw the entity. All drawing functions
    // should go here.
    void OnStageDraw();
    // Occurs when it is time to draw the entity in the editor.
    void OnEditorDraw();
    // Sets up any variables to be edited through the editor.
    static void OnSetup();

    // Functions
    // ==============================================

    // Statuses
    // ==============================================

    void Status_Normal();
};
