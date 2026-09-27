package dev.grandteleportruntimetest;

import com.mojang.brigadier.arguments.StringArgumentType;
import dev.codex.gtaliketeleport.GtaLikeTeleportServer;
import net.minecraft.commands.Commands;
import net.minecraft.core.BlockPos;
import net.minecraft.network.chat.Component;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.world.entity.RelativeMovement;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.level.block.SlabBlock;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.block.state.properties.SlabType;
import net.neoforged.bus.api.IEventBus;
import net.neoforged.bus.api.SubscribeEvent;
import net.neoforged.fml.ModContainer;
import net.neoforged.fml.common.Mod;
import net.neoforged.neoforge.common.NeoForge;
import net.neoforged.neoforge.event.RegisterCommandsEvent;

import java.util.Map;
import java.util.Set;
import java.util.UUID;
import java.util.concurrent.ConcurrentHashMap;

@Mod(GrandTeleportRuntimeTest.MOD_ID)
public final class GrandTeleportRuntimeTest {
    public static final String MOD_ID = "grandteleportruntimetest";
    public static final double SOURCE_X = 0.5D;
    public static final double SOURCE_Y = 100.0D;
    public static final double SOURCE_Z = 0.5D;
    public static final double DEST_X = 64.5D;
    public static final double DEST_Y = 100.0D;
    public static final double DEST_Z = 0.5D;

    private static final int LOW_ROOF_Y = 110;          // eye ~101.62 => ~8.38 blocks
    private static final int COMPRESSED_ROOF_Y = 126;  // eye ~101.62 => ~24.38 blocks
    private static final int SLAB_UNDER10_Y = 111;     // top slab starts at 111.5 => ~9.88
    private static final int SLAB_OVER10_Y = 112;      // bottom slab starts at 112.0 => ~10.38

    private static final Map<UUID, String> ACTIVE_SCENARIOS = new ConcurrentHashMap<>();

    public GrandTeleportRuntimeTest(IEventBus modBus, ModContainer container) {
        NeoForge.EVENT_BUS.register(this);
    }

    @SubscribeEvent
    public void onRegisterCommands(RegisterCommandsEvent event) {
        event.getDispatcher().register(Commands.literal("gttest")
                .then(Commands.literal("prepare")
                        .then(Commands.argument("scenario", StringArgumentType.word())
                                .executes(context -> {
                                    ServerPlayer player = context.getSource().getPlayerOrException();
                                    String scenario = StringArgumentType.getString(context, "scenario");
                                    prepare(player, scenario);
                                    context.getSource().sendSuccess(() -> Component.literal("GTTEST PREPARED " + scenario), false);
                                    return 1;
                                })))
                .then(Commands.literal("go")
                        .executes(context -> {
                            ServerPlayer player = context.getSource().getPlayerOrException();
                            String scenario = ACTIVE_SCENARIOS.get(player.getUUID());
                            if (scenario == null) {
                                context.getSource().sendFailure(Component.literal("GTTEST not prepared"));
                                return 0;
                            }
                            player.teleportTo(player.serverLevel(), DEST_X, DEST_Y, DEST_Z, Set.<RelativeMovement>of(), 0.0F, 0.0F);
                            context.getSource().sendSuccess(() -> Component.literal("GTTEST GO " + scenario), false);
                            return 1;
                        })));
    }

    private static void prepare(ServerPlayer player, String scenario) {
        ServerLevel level = player.serverLevel();
        clearArena(level, 0, 0);
        clearArena(level, 64, 0);
        buildFloor(level, 0, 0);
        buildFloor(level, 64, 0);

        switch (scenario) {
            case "low_origin" -> buildFullRoof(level, 0, 0, LOW_ROOF_Y);
            case "low_destination" -> buildFullRoof(level, 64, 0, LOW_ROOF_Y);
            case "low_both" -> {
                buildFullRoof(level, 0, 0, LOW_ROOF_Y);
                buildFullRoof(level, 64, 0, LOW_ROOF_Y);
            }
            case "compressed_both" -> {
                buildFullRoof(level, 0, 0, COMPRESSED_ROOF_Y);
                buildFullRoof(level, 64, 0, COMPRESSED_ROOF_Y);
            }
            case "slab_under10" -> buildSlabRoof(level, 0, 0, SLAB_UNDER10_Y, SlabType.TOP);
            case "slab_over10" -> buildSlabRoof(level, 0, 0, SLAB_OVER10_Y, SlabType.BOTTOM);
            case "open_open", "speed_slow", "speed_fast" -> {
                // fully open by construction
            }
            default -> throw new IllegalArgumentException("Unknown GTTEST scenario: " + scenario);
        }

        ACTIVE_SCENARIOS.put(player.getUUID(), scenario);

        // Setup movement must not itself trigger the GrandTeleport animation.
        GtaLikeTeleportServer.markNextServerTeleportBypassed(player);
        player.teleportTo(level, SOURCE_X, SOURCE_Y, SOURCE_Z, Set.<RelativeMovement>of(), 0.0F, 0.0F);
    }

    private static void clearArena(ServerLevel level, int centerX, int centerZ) {
        BlockPos.MutableBlockPos pos = new BlockPos.MutableBlockPos();
        for (int x = centerX - 3; x <= centerX + 3; x++) {
            for (int z = centerZ - 3; z <= centerZ + 3; z++) {
                for (int y = 99; y <= 220; y++) {
                    pos.set(x, y, z);
                    level.setBlock(pos, Blocks.AIR.defaultBlockState(), 3);
                }
            }
        }
    }

    private static void buildFloor(ServerLevel level, int centerX, int centerZ) {
        BlockPos.MutableBlockPos pos = new BlockPos.MutableBlockPos();
        for (int x = centerX - 3; x <= centerX + 3; x++) {
            for (int z = centerZ - 3; z <= centerZ + 3; z++) {
                pos.set(x, 99, z);
                level.setBlock(pos, Blocks.STONE.defaultBlockState(), 3);
            }
        }
    }

    private static void buildFullRoof(ServerLevel level, int centerX, int centerZ, int y) {
        BlockPos.MutableBlockPos pos = new BlockPos.MutableBlockPos();
        for (int x = centerX - 2; x <= centerX + 2; x++) {
            for (int z = centerZ - 2; z <= centerZ + 2; z++) {
                pos.set(x, y, z);
                level.setBlock(pos, Blocks.STONE.defaultBlockState(), 3);
            }
        }
    }

    private static void buildSlabRoof(ServerLevel level, int centerX, int centerZ, int y, SlabType type) {
        BlockState slab = Blocks.OAK_SLAB.defaultBlockState().setValue(SlabBlock.TYPE, type);
        BlockPos.MutableBlockPos pos = new BlockPos.MutableBlockPos();
        for (int x = centerX - 2; x <= centerX + 2; x++) {
            for (int z = centerZ - 2; z <= centerZ + 2; z++) {
                pos.set(x, y, z);
                level.setBlock(pos, slab, 3);
            }
        }
    }
}
