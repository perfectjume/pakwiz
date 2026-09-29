package dev.coreui.routertest;

import net.minecraft.client.Minecraft;
import net.minecraft.client.gui.screens.PauseScreen;
import net.minecraft.client.gui.screens.Screen;
import net.minecraft.client.gui.screens.achievement.StatsScreen;
import net.minecraft.client.gui.screens.advancements.AdvancementsScreen;
import net.minecraft.client.gui.screens.options.OptionsScreen;
import net.neoforged.api.distmarker.Dist;
import net.neoforged.fml.common.Mod;
import net.neoforged.neoforge.client.event.ClientTickEvent;
import net.neoforged.neoforge.common.NeoForge;

import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardOpenOption;

@Mod(value = ActionRouterRuntimeTest.MOD_ID, dist = Dist.CLIENT)
public final class ActionRouterRuntimeTest {
    public static final String MOD_ID = "coreuiroutertest";
    private int ticks;
    private int phase;
    private int phaseTicks;

    public ActionRouterRuntimeTest() {
        NeoForge.EVENT_BUS.addListener(this::onTick);
        trace("TEST_MOD_CONSTRUCTED");
    }

    private static boolean overlay(Minecraft mc) {
        return mc.screen != null && mc.screen.getClass().getName().equals("dev.coreui.bridge.GpuOverlayScreen");
    }

    private void onTick(ClientTickEvent.Post event) {
        Minecraft mc = Minecraft.getInstance();
        try {
            ticks++;
            phaseTicks++;

            if (phase == 0) {
                if (mc.level != null && mc.player != null && mc.screen == null) {
                    if (phaseTicks == 1) trace("IN_GAME=true");
                    if (phaseTicks >= 40) {
                        trace("OPEN_INITIAL_PAUSE");
                        mc.setScreen(new PauseScreen(true));
                        phase = 1;
                        phaseTicks = 0;
                    }
                } else {
                    phaseTicks = 0;
                    if (ticks > 3600) fail(mc, "IN_GAME_TIMEOUT");
                }
                return;
            }

            if (phase == 1) {
                if (mc.screen instanceof OptionsScreen) {
                    trace("OPTIONS_OPEN=PASS");
                    phase = 2;
                    phaseTicks = 0;
                } else if (phaseTicks > 240) fail(mc, "OPTIONS_OPEN_TIMEOUT");
                return;
            }

            if (phase == 2) {
                if (!(mc.screen instanceof OptionsScreen)) {
                    fail(mc, "OPTIONS_DISAPPEARED_EARLY");
                    return;
                }
                if (phaseTicks >= 10) {
                    Screen now = mc.screen;
                    boolean consumed = now.keyPressed(256, 0, 0);
                    trace("OPTIONS_ESC_SENT consumed=" + consumed);
                    phase = 3;
                    phaseTicks = 0;
                }
                return;
            }

            if (phase == 3) {
                if (overlay(mc)) {
                    trace("OPTIONS_RETURN_OVERLAY=PASS");
                    phase = 4;
                    phaseTicks = 0;
                } else if (mc.screen instanceof AdvancementsScreen) {
                    fail(mc, "ADVANCEMENTS_ARRIVED_BEFORE_OVERLAY_RETURN_OBSERVED");
                } else if (phaseTicks > 120) fail(mc, "OPTIONS_RETURN_TIMEOUT");
                return;
            }

            if (phase == 4) {
                if (mc.screen instanceof OptionsScreen) {
                    fail(mc, "STALE_ACTION_EXECUTED");
                    return;
                }
                if (mc.screen instanceof AdvancementsScreen) {
                    trace("STALE_ACTION_REJECTED=PASS");
                    trace("ADVANCEMENTS_OPEN=PASS");
                    phase = 5;
                    phaseTicks = 0;
                } else if (phaseTicks > 240) fail(mc, "ADVANCEMENTS_OPEN_TIMEOUT");
                return;
            }

            if (phase == 5) {
                if (!(mc.screen instanceof AdvancementsScreen)) {
                    fail(mc, "ADVANCEMENTS_DISAPPEARED_EARLY");
                    return;
                }
                if (phaseTicks >= 10) {
                    boolean consumed = mc.screen.keyPressed(256, 0, 0);
                    trace("ADVANCEMENTS_ESC_SENT consumed=" + consumed);
                    phase = 6;
                    phaseTicks = 0;
                }
                return;
            }

            if (phase == 6) {
                if (overlay(mc)) {
                    trace("ADVANCEMENTS_RETURN_OVERLAY=PASS");
                    phase = 7;
                    phaseTicks = 0;
                } else if (phaseTicks > 120) fail(mc, "ADVANCEMENTS_RETURN_TIMEOUT");
                return;
            }

            if (phase == 7) {
                if (mc.screen instanceof StatsScreen) {
                    trace("STATISTICS_OPEN=PASS");
                    phase = 8;
                    phaseTicks = 0;
                } else if (phaseTicks > 240) fail(mc, "STATISTICS_OPEN_TIMEOUT");
                return;
            }

            if (phase == 8) {
                if (!(mc.screen instanceof StatsScreen)) {
                    fail(mc, "STATISTICS_DISAPPEARED_EARLY");
                    return;
                }
                if (phaseTicks >= 10) {
                    boolean consumed = mc.screen.keyPressed(256, 0, 0);
                    trace("STATISTICS_ESC_SENT consumed=" + consumed);
                    phase = 9;
                    phaseTicks = 0;
                }
                return;
            }

            if (phase == 9) {
                if (overlay(mc)) {
                    trace("STATISTICS_RETURN_OVERLAY=PASS");
                    trace("ACTION_ROUTER_RUNTIME_TEST_RESULT PASS");
                    phase = 99;
                    mc.setScreen(null);
                    mc.stop();
                } else if (phaseTicks > 120) fail(mc, "STATISTICS_RETURN_TIMEOUT");
            }
        } catch (Throwable t) {
            trace("EXCEPTION=" + t.getClass().getName() + ":" + String.valueOf(t.getMessage()));
            t.printStackTrace();
            phase = 99;
            try { mc.stop(); } catch (Throwable ignored) {}
        }
    }

    private static void fail(Minecraft mc, String reason) {
        trace("ACTION_ROUTER_RUNTIME_TEST_RESULT FAIL " + reason);
        try { mc.stop(); } catch (Throwable ignored) {}
    }

    private static synchronized void trace(String s) {
        System.out.println("[CORE-UI-ACTION-TEST] " + s);
        try {
            Files.writeString(
                Path.of("CORE_UI_ACTION_ROUTER_TRACE.txt"),
                s + System.lineSeparator(),
                StandardOpenOption.CREATE,
                StandardOpenOption.APPEND
            );
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
    }
}
