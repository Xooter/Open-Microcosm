#pragma once

class World;

struct UiActions {
    bool randomizeRules = false;
    bool respawn = false;
};

// Panel de debug para tunear las reglas en vivo.
UiActions drawUi(World& world, bool& paused);
