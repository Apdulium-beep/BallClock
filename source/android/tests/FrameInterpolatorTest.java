package local.ballclock.controller;
import java.util.Arrays;
public final class FrameInterpolatorTest {
    public static void main(String[] args) {
        FrameInterpolator animation=new FrameInterpolator();
        int[] black=new int[128],white=new int[128],sample=new int[128];
        Arrays.fill(black,0xff000000); Arrays.fill(white,0xffffffff);
        animation.accept(black,0); animation.sample(0,sample);
        if(sample[0]!=0xff000000) throw new AssertionError("Initial frame changed");
        animation.accept(white,100); animation.sample(100,sample);
        if(sample[0]!=0xff000000) throw new AssertionError("Network sample caused a visual jump");
        animation.sample(140,sample);
        if(sample[0]!=0xff808080) throw new AssertionError("Expected smooth midpoint");
        animation.sample(180,sample);
        if(sample[0]!=0xffffffff) throw new AssertionError("Target not reached");
        animation.accept(black,180); animation.sample(220,sample);
        if(sample[0]!=0xff808080) throw new AssertionError("Reverse transition failed");
        int midpoint=sample[0];animation.accept(white,220);animation.sample(220,sample);
        if(sample[0]!=midpoint) throw new AssertionError("Interrupted transition jumped");
        animation.sample(10000,sample);
        if(sample[0]!=0xffffffff) throw new AssertionError("Transition never settled");
        animation.clear();if(animation.isReady()) throw new AssertionError("Offline frame not cleared");
        System.out.println("PASS: frame interpolation has continuous RGB transitions, jitter bounds and exact endpoints.");
    }
}
