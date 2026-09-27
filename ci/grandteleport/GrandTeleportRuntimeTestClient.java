package dev.grandteleportruntimetest;

import dev.codex.gtaliketeleport.TeleportTransitionController;
import net.minecraft.client.Minecraft;
import net.minecraft.client.gui.screens.ConnectScreen;
import net.minecraft.client.gui.screens.TitleScreen;
import net.minecraft.client.multiplayer.ServerAddress;
import net.minecraft.client.multiplayer.ServerData;
import net.minecraft.world.phys.Vec3;
import net.neoforged.api.distmarker.Dist;
import net.neoforged.bus.api.IEventBus;
import net.neoforged.fml.ModContainer;
import net.neoforged.fml.common.Mod;
import net.neoforged.neoforge.common.NeoForge;
import net.neoforged.neoforge.client.event.ClientTickEvent;

import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardOpenOption;
import java.util.List;

@Mod(value = GrandTeleportRuntimeTest.MOD_ID, dist = Dist.CLIENT)
public final class GrandTeleportRuntimeTestClient {
    private enum DriverState { WAIT_JOIN, PREPARE, WAIT_PREPARED, START, RUN, FINISH }

    private record Scenario(
            String name,
            boolean expectOrigin,
            boolean expectDestination,
            double originRoofCollisionY,
            double destinationRoofCollisionY,
            double speed,
            double minOriginTop,
            double maxOriginTop,
            double minDestinationTop,
            double maxDestinationTop
    ) {}

    private static final double INF = Double.POSITIVE_INFINITY;
    private static final List<Scenario> SCENARIOS = List.of(
            new Scenario("open_open", true, true, INF, INF, 30.77D, 15.0D, INF, 15.0D, INF),
            new Scenario("low_origin", false, true, 110.0D, INF, 30.77D, 0.0D, 0.0D, 15.0D, INF),
            new Scenario("low_destination", true, false, INF, 110.0D, 30.77D, 15.0D, INF, 0.0D, 0.0D),
            new Scenario("low_both", false, false, 110.0D, 110.0D, 30.77D, 0.0D, 0.0D, 0.0D, 0.0D),
            new Scenario("compressed_both", true, true, 126.0D, 126.0D, 30.77D, 18.0D, 24.25D, 18.0D, 24.25D),
            new Scenario("slab_under10", false, true, 111.5D, INF, 30.77D, 0.0D, 0.0D, 15.0D, INF),
            new Scenario("slab_over10", true, true, 112.0D, INF, 30.77D, 8.8D, 10.20D, 15.0D, INF),
            new Scenario("speed_slow", true, true, INF, INF, 10.0D, 15.0D, INF, 15.0D, INF),
            new Scenario("speed_fast", true, true, INF, INF, 80.0D, 15.0D, INF, 15.0D, INF)
    );

    private DriverState state = DriverState.WAIT_JOIN;
    private int globalTicks;
    private int stateTicks;
    private boolean connectAttempted;
    private int scenarioIndex;
    private Scenario scenario;
    private Vec3 sourceEye;
    private Vec3 destinationEye;
    private int originFrames;
    private int destinationFrames;
    private int controllerFrames;
    private int actualMatchedFrames;
    private double maxOriginOffset;
    private double maxDestinationOffset;
    private boolean sawRunning;
    private boolean playerArrived;
    private boolean horizontalViolation;
    private boolean collisionViolation;
    private boolean actualHorizontalViolation;
    private boolean actualCameraObserved;
    private int slowOriginFrames = -1;
    private int fastOriginFrames = -1;

    public GrandTeleportRuntimeTestClient(IEventBus modBus, ModContainer container) {
        NeoForge.EVENT_BUS.addListener(this::onClientTick);
        trace("CLIENT_TEST_CONSTRUCTED");
    }

    private void onClientTick(ClientTickEvent.Post event) {
        try {
            Minecraft mc = Minecraft.getInstance();
            globalTicks++;
            stateTicks++;

            if (state == DriverState.WAIT_JOIN) {
                if (!connectAttempted && globalTicks >= 140 && mc.player == null && mc.getConnection() == null) {
                    connectAttempted = true;
                    trace("CONNECT_ATTEMPT localhost:25565");
                    String address = "127.0.0.1:25565";
                    ServerData data = new ServerData("GrandTeleport Runtime Server", address, ServerData.Type.OTHER);
                    ConnectScreen.startConnecting(new TitleScreen(), mc, ServerAddress.parseString(address), data, false, null);
                    return;
                }
                if (mc.player != null && mc.level != null && mc.getConnection() != null && globalTicks > 100) {
                    trace("CLIENT_JOINED position=" + fmt(mc.player.position()));
                    Files.deleteIfExists(mc.gameDirectory.toPath().resolve("config/grand_teleport_vertical.properties"));
                    scenarioIndex = 0;
                    startPrepare(mc);
                } else if (connectAttempted && globalTicks > 500) {
                    fail(mc, "JOIN_TIMEOUT");
                }
                return;
            }

            if (mc.player == null || mc.level == null || mc.getConnection() == null) {
                return;
            }

            if (state == DriverState.WAIT_PREPARED) {
                if (near(mc.player.position(), GrandTeleportRuntimeTest.SOURCE_X, GrandTeleportRuntimeTest.SOURCE_Y, GrandTeleportRuntimeTest.SOURCE_Z, 1.0D)
                        && arenaLoaded(mc, scenario)) {
                    if (stateTicks >= 40) {
                        state = DriverState.START;
                        stateTicks = 0;
                    }
                } else if (stateTicks > 240) {
                    fail(mc, "PREPARE_TIMEOUT " + scenario.name());
                }
                return;
            }

            if (state == DriverState.START) {
                writeSpeed(mc, scenario.speed());
                sourceEye = mc.player.getEyePosition();
                resetMetrics();
                trace("SCENARIO_START " + scenario.name() + " sourceEye=" + fmt(sourceEye) + " speed=" + scenario.speed());
                mc.getConnection().sendCommand("gttest go");
                state = DriverState.RUN;
                stateTicks = 0;
                return;
            }

            if (state == DriverState.RUN) {
                sample(mc);
                boolean running = TeleportTransitionController.isRunning();
                if (running) {
                    sawRunning = true;
                }

                if (!playerArrived && near(mc.player.position(), GrandTeleportRuntimeTest.DEST_X, GrandTeleportRuntimeTest.DEST_Y, GrandTeleportRuntimeTest.DEST_Z, 2.0D)) {
                    playerArrived = true;
                    destinationEye = mc.player.getEyePosition();
                    trace("PLAYER_ARRIVED " + scenario.name() + " destinationEye=" + fmt(destinationEye));
                }

                if (playerArrived && !running && stateTicks >= 10) {
                    evaluateScenario(mc);
                    return;
                }

                if (stateTicks > 700) {
                    fail(mc, "SCENARIO_TIMEOUT " + scenario.name());
                }
                return;
            }

            if (state == DriverState.FINISH) {
                // no-op; mc.stop() was requested
            }
        } catch (Throwable t) {
            trace("EXCEPTION " + t.getClass().getName() + ": " + String.valueOf(t.getMessage()));
            t.printStackTrace();
            try { fail(Minecraft.getInstance(), "EXCEPTION " + t.getClass().getSimpleName()); } catch (Throwable ignored) {}
        }
    }

    private void startPrepare(Minecraft mc) {
        scenario = SCENARIOS.get(scenarioIndex);
        state = DriverState.WAIT_PREPARED;
        stateTicks = 0;
        trace("PREPARE " + scenario.name());
        mc.getConnection().sendCommand("gttest prepare " + scenario.name());
    }

    private void resetMetrics() {
        destinationEye = null;
        originFrames = 0;
        destinationFrames = 0;
        controllerFrames = 0;
        actualMatchedFrames = 0;
        maxOriginOffset = 0.0D;
        maxDestinationOffset = 0.0D;
        sawRunning = false;
        playerArrived = false;
        horizontalViolation = false;
        collisionViolation = false;
        actualHorizontalViolation = false;
        actualCameraObserved = false;
    }

    private void sample(Minecraft mc) {
        TeleportTransitionController.CameraFrame frame = TeleportTransitionController.getCameraFrame(0.0F);
        if (frame == null) {
            return;
        }

        controllerFrames++;
        Vec3 p = frame.pos();
        boolean atSource = nearXZ(p, GrandTeleportRuntimeTest.SOURCE_X, GrandTeleportRuntimeTest.SOURCE_Z, 0.02D);
        boolean atDest = nearXZ(p, GrandTeleportRuntimeTest.DEST_X, GrandTeleportRuntimeTest.DEST_Z, 0.02D);
        if (!atSource && !atDest) {
            horizontalViolation = true;
            trace("HORIZONTAL_CONTROLLER_VIOLATION " + scenario.name() + " pos=" + fmt(p));
        }

        if (atSource && sourceEye != null) {
            originFrames++;
            maxOriginOffset = Math.max(maxOriginOffset, p.y - sourceEye.y);
            if (Double.isFinite(scenario.originRoofCollisionY()) && p.y + 0.25001D > scenario.originRoofCollisionY()) {
                collisionViolation = true;
                trace("ORIGIN_COLLISION_VIOLATION " + scenario.name() + " cameraY=" + p.y + " roof=" + scenario.originRoofCollisionY());
            }
        } else if (atDest) {
            if (destinationEye == null && mc.player != null && near(mc.player.position(), GrandTeleportRuntimeTest.DEST_X, GrandTeleportRuntimeTest.DEST_Y, GrandTeleportRuntimeTest.DEST_Z, 2.0D)) {
                destinationEye = mc.player.getEyePosition();
            }
            if (destinationEye != null) {
                destinationFrames++;
                maxDestinationOffset = Math.max(maxDestinationOffset, p.y - destinationEye.y);
                if (Double.isFinite(scenario.destinationRoofCollisionY()) && p.y + 0.25001D > scenario.destinationRoofCollisionY()) {
                    collisionViolation = true;
                    trace("DEST_COLLISION_VIOLATION " + scenario.name() + " cameraY=" + p.y + " roof=" + scenario.destinationRoofCollisionY());
                }
            }
        }

        // This is the actual Camera instance after GrandTeleport's CameraMixin has
        // applied the last rendered frame. A one-tick Y lag is tolerated, but X/Z
        // must still be at one endpoint: never between the two teleport positions.
        Vec3 actual = mc.gameRenderer.getMainCamera().getPosition();
        if (actual != null && actual.distanceTo(p) < 8.0D) {
            actualMatchedFrames++;
            actualCameraObserved = true;
            boolean actualSource = nearXZ(actual, GrandTeleportRuntimeTest.SOURCE_X, GrandTeleportRuntimeTest.SOURCE_Z, 0.35D);
            boolean actualDest = nearXZ(actual, GrandTeleportRuntimeTest.DEST_X, GrandTeleportRuntimeTest.DEST_Z, 0.35D);
            if (!actualSource && !actualDest) {
                actualHorizontalViolation = true;
                trace("HORIZONTAL_RENDER_CAMERA_VIOLATION " + scenario.name() + " actual=" + fmt(actual));
            }
        }
    }

    private void evaluateScenario(Minecraft mc) throws Exception {
        trace("SCENARIO_METRICS " + scenario.name()
                + " originFrames=" + originFrames
                + " destinationFrames=" + destinationFrames
                + " controllerFrames=" + controllerFrames
                + " actualMatchedFrames=" + actualMatchedFrames
                + " maxOriginOffset=" + maxOriginOffset
                + " maxDestinationOffset=" + maxDestinationOffset
                + " sawRunning=" + sawRunning);

        require(!horizontalViolation, "controller moved horizontally");
        require(!actualHorizontalViolation, "rendered Camera moved horizontally");
        require(!collisionViolation, "camera entered roof collision volume");

        if (scenario.expectOrigin()) {
            require(originFrames >= 2, "origin animation missing");
            require(maxOriginOffset >= scenario.minOriginTop(), "origin zoom too small: " + maxOriginOffset);
            require(maxOriginOffset <= scenario.maxOriginTop(), "origin zoom exceeds safe top: " + maxOriginOffset);
        } else {
            require(originFrames == 0, "origin should have been skipped but frames=" + originFrames);
        }

        if (scenario.expectDestination()) {
            require(destinationFrames >= 2, "destination animation missing");
            require(maxDestinationOffset >= scenario.minDestinationTop(), "destination zoom too small: " + maxDestinationOffset);
            require(maxDestinationOffset <= scenario.maxDestinationTop(), "destination zoom exceeds safe top: " + maxDestinationOffset);
        } else {
            require(destinationFrames == 0, "destination should have been skipped but frames=" + destinationFrames);
        }

        if (scenario.expectOrigin() || scenario.expectDestination()) {
            require(actualCameraObserved, "actual rendered Camera never matched controller output");
        }

        Path config = mc.gameDirectory.toPath().resolve("config/grand_teleport_vertical.properties");
        require(Files.isRegularFile(config), "vertical zoom config file was not created");
        String configText = Files.readString(config);
        require(configText.contains("zoomSpeedBlocksPerSecond="), "zoom speed key missing from config");

        if (scenario.name().equals("speed_slow")) slowOriginFrames = originFrames;
        if (scenario.name().equals("speed_fast")) fastOriginFrames = originFrames;

        trace("SCENARIO_PASS " + scenario.name());
        scenarioIndex++;
        if (scenarioIndex >= SCENARIOS.size()) {
            require(slowOriginFrames > 0 && fastOriginFrames > 0, "speed scenarios did not record frames");
            require(slowOriginFrames >= fastOriginFrames * 3, "zoomSpeed config did not materially change runtime duration: slow=" + slowOriginFrames + " fast=" + fastOriginFrames);
            trace("SPEED_COMPARISON_PASS slow=" + slowOriginFrames + " fast=" + fastOriginFrames);
            finish(mc);
        } else {
            startPrepare(mc);
        }
    }

    private static boolean arenaLoaded(Minecraft mc, Scenario scenario) {
        if (mc.level == null) return false;
        // Verify the client actually received the decisive roof block/shape before starting.
        if (scenario.name().equals("low_origin")) return !mc.level.getBlockState(new net.minecraft.core.BlockPos(0, 110, 0)).isAir();
        if (scenario.name().equals("low_destination")) return !mc.level.getBlockState(new net.minecraft.core.BlockPos(64, 110, 0)).isAir();
        if (scenario.name().equals("low_both")) return !mc.level.getBlockState(new net.minecraft.core.BlockPos(0, 110, 0)).isAir() && !mc.level.getBlockState(new net.minecraft.core.BlockPos(64, 110, 0)).isAir();
        if (scenario.name().equals("compressed_both")) return !mc.level.getBlockState(new net.minecraft.core.BlockPos(0, 126, 0)).isAir() && !mc.level.getBlockState(new net.minecraft.core.BlockPos(64, 126, 0)).isAir();
        if (scenario.name().equals("slab_under10")) return !mc.level.getBlockState(new net.minecraft.core.BlockPos(0, 111, 0)).isAir();
        if (scenario.name().equals("slab_over10")) return !mc.level.getBlockState(new net.minecraft.core.BlockPos(0, 112, 0)).isAir();
        return true;
    }

    private static void writeSpeed(Minecraft mc, double speed) throws Exception {
        Path config = mc.gameDirectory.toPath().resolve("config/grand_teleport_vertical.properties");
        Files.createDirectories(config.getParent());
        Files.writeString(config, "zoomSpeedBlocksPerSecond=" + speed + System.lineSeparator());
        trace("CONFIG_SPEED " + speed);
    }

    private static boolean near(Vec3 p, double x, double y, double z, double radius) {
        double dx = p.x - x, dy = p.y - y, dz = p.z - z;
        return dx * dx + dy * dy + dz * dz <= radius * radius;
    }

    private static boolean nearXZ(Vec3 p, double x, double z, double radius) {
        double dx = p.x - x, dz = p.z - z;
        return dx * dx + dz * dz <= radius * radius;
    }

    private static String fmt(Vec3 p) {
        return String.format(java.util.Locale.ROOT, "(%.3f,%.3f,%.3f)", p.x, p.y, p.z);
    }

    private static void require(boolean condition, String message) {
        if (!condition) throw new IllegalStateException(message);
    }

    private static synchronized void trace(String line) {
        System.out.println("[GRANDTP-RUNTIME] " + line);
        try {
            Minecraft mc = Minecraft.getInstance();
            Path base = mc.gameDirectory == null ? Path.of(".") : mc.gameDirectory.toPath();
            Files.writeString(base.resolve("GRANDTP_RUNTIME_TRACE.txt"), line + System.lineSeparator(), StandardOpenOption.CREATE, StandardOpenOption.APPEND);
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
    }

    private static void fail(Minecraft mc, String reason) {
        trace("CLIENT_RUNTIME_TEST_RESULT FAIL " + reason);
        try {
            Files.writeString(mc.gameDirectory.toPath().resolve("GRANDTP_RUNTIME_RESULT.txt"), "CLIENT_RUNTIME_TEST_RESULT FAIL " + reason + System.lineSeparator());
        } catch (Exception ignored) {}
        mc.stop();
    }

    private static void finish(Minecraft mc) {
        stateStop();
        trace("CLIENT_RUNTIME_TEST_RESULT PASS");
        try {
            Files.writeString(mc.gameDirectory.toPath().resolve("GRANDTP_RUNTIME_RESULT.txt"), "CLIENT_RUNTIME_TEST_RESULT PASS" + System.lineSeparator());
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
        mc.stop();
    }

    private static void stateStop() {
        // Method exists only to keep the final transition explicit in traces.
    }
}
