tag @s add hexpectate
execute as @a[distance=..10] run tag @s remove hextarget
power revoke @s apocalypse:hexblade/hexblade_spectating_controls hexblade_bars
power revoke @a[distance=..10,sort=nearest,limit=1,tag=!hexblade] apocalypse:hexblade/hexfriend_powers hexblade_bars
tag @s remove hexpectate
tag @s remove fail
execute as @s run gamemode survival