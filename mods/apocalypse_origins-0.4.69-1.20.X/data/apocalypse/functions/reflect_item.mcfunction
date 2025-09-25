item replace entity @s weapon.mainhand from entity @e[type=!item,distance=0.1..10,limit=1,sort=nearest] weapon.mainhand
item modify entity @s weapon.mainhand apocalypse:mirror_tag
execute unless data entity @s SelectedItem.tag.Damage run item replace entity @s weapon.mainhand with minecraft:air
execute unless data entity @s SelectedItem.tag.mirror run function apocalypse:reflect_item_clear