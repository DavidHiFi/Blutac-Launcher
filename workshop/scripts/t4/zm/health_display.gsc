// Health display for World at War Zombies (Plutonium T4).
// Shows your current health on the left side of the screen.
// Blutac Launcher installs it to storage/t4/raw/scripts/sp.

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
    label SetText("Health");
    value = make_text(126, "left");

    for (;;)
    {
        value SetValue(self.health);
        wait 0.1;
    }
}

make_text(x, alignX)
{
    hud = NewClientHudElem(self);
    hud.alignX = alignX;
    hud.alignY = "middle";
    hud.horzAlign = "left";
    hud.vertAlign = "middle";
    hud.x = x;
    hud.y = 0;
    hud.fontScale = 1.4;
    return hud;
}
