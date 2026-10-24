// Zombie counter for Black Ops Zombies (Plutonium T5).
// Shows how many zombies are left this round: the ones alive plus the
// ones still waiting to spawn.
// Blutac Launcher installs it to storage/t5/raw/scripts/sp/zom.

init()
{
    level thread on_player_connect();
}

on_player_connect()
{
    for (;;)
    {
        level waittill("connecting", player);
        player thread zombie_hud();
    }
}

zombie_hud()
{
    self endon("disconnect");
    self waittill("spawned_player");

    label = make_text(120, "right");
    label SetText("Zombies left");
    value = make_text(126, "left");

    for (;;)
    {
        value SetValue(zombies_left());
        wait 0.25;
    }
}

zombies_left()
{
    alive = GetAiSpeciesArray("axis", "all").size;
    if (IsDefined(level.zombie_total))
        return alive + level.zombie_total;
    return alive;
}

make_text(x, alignX)
{
    hud = NewClientHudElem(self);
    hud.alignX = alignX;
    hud.alignY = "middle";
    hud.horzAlign = "user_left";
    hud.vertAlign = "user_center";
    hud.x = x;
    hud.y = 20;
    hud.fontScale = 1.4;
    hud.hidewheninmenu = true;
    return hud;
}
