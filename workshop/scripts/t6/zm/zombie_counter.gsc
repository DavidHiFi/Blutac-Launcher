// Zombie counter for Black Ops II Zombies (Plutonium T6).
// Shows how many zombies are left this round: the ones alive plus the
// ones still waiting to spawn.
// Blutac Launcher installs it to storage/t6/scripts/zm.

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
    label settext("Zombies left");
    value = make_text(126, "left");

    for (;;)
    {
        value setvalue(zombies_left());
        wait 0.25;
    }
}

zombies_left()
{
    alive = getaispeciesarray(level.zombie_team, "all").size;
    if (isdefined(level.zombie_total))
        return alive + level.zombie_total;
    return alive;
}

make_text(x, alignx)
{
    hud = newclienthudelem(self);
    hud.alignx = alignx;
    hud.aligny = "middle";
    hud.horzalign = "user_left";
    hud.vertalign = "user_center";
    hud.x = x;
    hud.y = 20;
    hud.fontscale = 1.4;
    hud.hidewheninmenu = true;
    return hud;
}
