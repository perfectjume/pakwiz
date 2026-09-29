package dev.coreui.settingstest;

import net.minecraft.client.GraphicsStatus;
import net.minecraft.client.Minecraft;
import net.minecraft.client.gui.screens.PauseScreen;
import net.neoforged.api.distmarker.Dist;
import net.neoforged.fml.common.Mod;
import net.neoforged.neoforge.client.event.ClientTickEvent;
import net.neoforged.neoforge.common.NeoForge;

import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardOpenOption;

@Mod(value = SettingsRuntimeTest.MOD_ID, dist = Dist.CLIENT)
public final class SettingsRuntimeTest {
    public static final String MOD_ID = "coreuisettingstest";

    private int ticks;
    private int phase;
    private int phaseTicks;

    private boolean baseSubtitles;
    private boolean baseAutoJump;
    private int baseFov;
    private int baseRenderDistance;
    private GraphicsStatus baseGraphics;

    public SettingsRuntimeTest() {
        NeoForge.EVENT_BUS.addListener(this::onTick);
        trace("TEST_MOD_CONSTRUCTED");
    }

    private static boolean overlay(Minecraft mc) {
        return mc.screen != null && mc.screen.getClass().getName().equals("dev.coreui.bridge.GpuOverlayScreen");
    }

    private boolean allBaseline(Minecraft mc) {
        return mc.options.showSubtitles().get() == baseSubtitles
            && mc.options.autoJump().get() == baseAutoJump
            && mc.options.fov().get() == baseFov
            && mc.options.renderDistance().get() == baseRenderDistance
            && mc.options.graphicsMode().get() == baseGraphics;
    }

    private boolean allMutated(Minecraft mc) {
        return mc.options.showSubtitles().get() != baseSubtitles
            && mc.options.autoJump().get() != baseAutoJump
            && mc.options.fov().get() != baseFov
            && mc.options.renderDistance().get() != baseRenderDistance
            && mc.options.graphicsMode().get() != baseGraphics;
    }

    private void onTick(ClientTickEvent.Post event) {
        Minecraft mc = Minecraft.getInstance();
        try {
            ticks++;
            phaseTicks++;

            if (ticks > 4800) {
                fail(mc, "GLOBAL_TIMEOUT");
                return;
            }

            if (phase == 0) {
                if (mc.level != null && mc.player != null && mc.screen == null) {
                    if (phaseTicks == 1) {
                        baseSubtitles = mc.options.showSubtitles().get();
                        baseAutoJump = mc.options.autoJump().get();
                        baseFov = mc.options.fov().get();
                        baseRenderDistance = mc.options.renderDistance().get();
                        baseGraphics = mc.options.graphicsMode().get();
                        trace("BASELINE subtitles=" + baseSubtitles
                            + " autoJump=" + baseAutoJump
                            + " fov=" + baseFov
                            + " renderDistance=" + baseRenderDistance
                            + " graphics=" + baseGraphics.name());
                    }
                    if (phaseTicks >= 40) {
                        trace("OPEN_FIRST_PAUSE");
                        mc.setScreen(new PauseScreen(true));
                        phase = 1;
                        phaseTicks = 0;
                    }
                } else {
                    phaseTicks = 0;
                }
                return;
            }

            if (phase == 1) {
                if (allMutated(mc)) {
                    trace("DIRECT_OPTIONS_MUTATION=PASS");
                    phase = 2;
                    phaseTicks = 0;
                } else if (phaseTicks > 500) {
                    fail(mc, "MUTATION_TIMEOUT");
                }
                return;
            }

            if (phase == 2) {
                if (allBaseline(mc)) {
                    trace("DIRECT_OPTIONS_RESTORE=PASS");
                    phase = 3;
                    phaseTicks = 0;
                } else if (phaseTicks > 500) {
                    fail(mc, "RESTORE_TIMEOUT");
                }
                return;
            }

            if (phase == 3) {
                if (mc.screen == null) {
                    phase = 4;
                    phaseTicks = 0;
                } else if (phaseTicks > 160) {
                    fail(mc, "FIRST_RESUME_TIMEOUT");
                }
                return;
            }

            if (phase == 4) {
                if (phaseTicks >= 30) {
                    trace("OPEN_SECOND_PAUSE");
                    mc.setScreen(new PauseScreen(true));
                    phase = 5;
                    phaseTicks = 0;
                }
                return;
            }

            if (phase == 5) {
                if (!allBaseline(mc)) {
                    fail(mc, "STALE_SETTING_MUTATED_OPTIONS");
                    return;
                }
                if (mc.screen == null) {
                    trace("STALE_SETTING_DIRECT_REJECTION=PASS");
                    trace("SETTINGS_RUNTIME_TEST_RESULT PASS");
                    phase = 99;
                    mc.stop();
                } else if (phaseTicks > 400) {
                    fail(mc, "SECOND_SESSION_TIMEOUT");
                }
            }
        } catch (Throwable t) {
            trace("EXCEPTION=" + t.getClass().getName() + ":" + String.valueOf(t.getMessage()));
            t.printStackTrace();
            phase = 99;
            try { mc.stop(); } catch (Throwable ignored) {}
        }
    }

    private static void fail(Minecraft mc, String reason) {
        trace("SETTINGS_RUNTIME_TEST_RESULT FAIL " + reason);
        try { mc.stop(); } catch (Throwable ignored) {}
    }

    private static synchronized void trace(String s) {
        System.out.println("[CORE-UI-SETTINGS-TEST] " + s);
        try {
            Files.writeString(
                Path.of("CORE_UI_SETTINGS_RUNTIME_TRACE.txt"),
                s + System.lineSeparator(),
                StandardOpenOption.CREATE,
                StandardOpenOption.APPEND
            );
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
    }
}
