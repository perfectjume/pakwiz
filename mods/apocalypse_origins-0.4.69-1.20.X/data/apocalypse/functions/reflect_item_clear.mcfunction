item replace entity @s weapon.mainhand with minecraft:air
tellraw @s {"text":"Your mirror returns...","color":"dark_aqua"}
function apocalypse:mirror_give
power revoke @s apocalypse:refractor/refractor_reflect_item