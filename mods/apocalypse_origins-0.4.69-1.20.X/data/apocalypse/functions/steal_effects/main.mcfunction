#Checks entity for active effect
execute if data entity @s ActiveEffects[{Id:1}] run function apocalypse:steal_effects/speed
execute if data entity @s ActiveEffects[{Id:5}] run function apocalypse:steal_effects/strength
execute if data entity @s ActiveEffects[{Id:21}] run function apocalypse:steal_effects/health_boost
execute if data entity @s ActiveEffects[{Id:10}] run function apocalypse:steal_effects/regeneration
execute if data entity @s ActiveEffects[{Id:23}] run function apocalypse:steal_effects/saturation
execute if data entity @s ActiveEffects[{Id:11}] run function apocalypse:steal_effects/resistance
execute if data entity @s ActiveEffects[{Id:12}] run function apocalypse:steal_effects/fire_resistance
execute if data entity @s ActiveEffects[{Id:3}] run function apocalypse:steal_effects/haste
execute if data entity @s ActiveEffects[{Id:22}] run function apocalypse:steal_effects/absorption
#execute if data entity @s ActiveEffects[{Id:<EFFECTID>}] run function apocalypse:steal_effects/<EFFECTNAME>