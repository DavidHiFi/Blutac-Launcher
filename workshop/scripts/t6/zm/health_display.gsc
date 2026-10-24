// Health display for Black Ops II Zombies (Plutonium T6).
// Shows your current health on the left side of the screen.
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
        player thread health_hud();
    }
}

health_hud()
{
    self endon("disconnect");
    self waittill("spawned_player");

    label = make_text(120, "right");
    label settext("Health");
    value = make_text(126, "left");

    for (;;)
    {
        value setvalue(self.health);
        wait 0.1;
    }
}

make_text(x, alignx)
{
    hud = newclienthudelem(self);
    hud.alignx = alignx;
    hud.aligny = "middle";
    hud.horzalign = "user_left";
    hud.vertalign = "user_center";
    hud.x = x;
    hud.y = 0;
    hud.fontscale = 1.4;
    hud.hidewheninmenu = true;
    return hud;
}
