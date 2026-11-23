package local.ballclock.controller;

/** UI-only interpolation: never sends invented frames to the physical clock. */
public final class FrameInterpolator {
    private final int[] from = new int[ClockFrame.COUNT], target = new int[ClockFrame.COUNT];
    private boolean ready;
    private long started, duration = 70, previousSample;

    public void clear() { ready = false; previousSample = 0; }
    public boolean isReady() { return ready; }
    public void accept(int[] colors, long now) {
        if (colors == null || colors.length != ClockFrame.COUNT) throw new IllegalArgumentException("Invalid frame");
        if (ready) {
            sample(now, from);
            long interval = Math.max(35, Math.min(180, now - previousSample));
            duration = (duration * 2 + interval) / 3;
        } else {
            System.arraycopy(colors, 0, from, 0, colors.length);
            ready = true;
        }
        System.arraycopy(colors, 0, target, 0, colors.length);
        previousSample = now; started = now;
    }
    public boolean sample(long now, int[] result) {
        if (!ready) return false;
        float fraction = Math.max(0f, Math.min(1f, (now - started) / (float) duration));
        for (int led = 0; led < ClockFrame.COUNT; led++) {
            int rgb = 0xff000000;
            for (int shift = 0; shift <= 16; shift += 8) {
                int first = (from[led] >> shift) & 255, last = (target[led] >> shift) & 255;
                rgb |= Math.round(first + (last - first) * fraction) << shift;
            }
            result[led] = rgb;
        }
        return fraction < 1;
    }
}
