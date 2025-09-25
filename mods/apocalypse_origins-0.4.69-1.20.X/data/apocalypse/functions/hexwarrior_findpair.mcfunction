#  Store the score of the current player from the `hexbladeid` scoreboard objective to a temporary score holder
scoreboard players operation #hexwarrior hexhold = @s hexhold

#  Execute as all the players that has the `hexblade` tag, and check if their score from the `hexhold` scoreboard objective matches the score of the `#current` score holder from the `hexbladeid` scoreboard objective
execute as @a[tag = hexblade] if score @s hexbladeid = #hexwarrior hexhold run resource change @s apocalypse:hexblade/hexblade_resource_souls 1
execute as @a[tag = hexblade] if score @s hexbladeid = #hexwarrior hexhold run tellraw @a[tag=soulgain] {"text":"","color":"dark_purple","extra":[{"selector":"@s"},{"text":" received this soul"}]}
tag @s remove soulgain