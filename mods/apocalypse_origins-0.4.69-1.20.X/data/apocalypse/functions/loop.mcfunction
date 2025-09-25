execute as @a unless score @s hexbladeid = @s hexbladeid store result score @s hexbladeid run scoreboard players add #global hexbladeid 1
execute as @a[nbt={SelectedItem:{id:"minecraft:wooden_sword",tag:{CustomModelData:1}}}] store result score @s hexhold run data get entity @s SelectedItem.tag.hexbladeid
execute as @a[nbt={SelectedItem:{id:"minecraft:amethyst_shard",tag:{CustomModelData:1}}}] store result score @s hexhold run data get entity @s SelectedItem.tag.hexbladeid
execute as @a[nbt={SelectedItem:{id:"minecraft:emerald",tag:{CustomModelData:1}}}] store result score @s hexhold run data get entity @s SelectedItem.tag.hexbladeid
execute as @e[type=item,nbt={Item:{tag:{HideFlags:122}}}] store result score @s hexhold run data get entity @s Item.tag.hexbladeid