package local.ballclock.controller;
public final class AdaptiveLayoutTest {
    public static void main(String[] args) {
        for(int width:new int[]{320,380,600,768}) if(AdaptiveLayout.sideBySide(width))throw new AssertionError("Narrow split");
        for(int width:new int[]{800,960,1280,1800}) if(!AdaptiveLayout.sideBySide(width))throw new AssertionError("Tablet not split");
        if(AdaptiveLayout.dialHeight(600)<=AdaptiveLayout.dialHeight(380))throw new AssertionError("Tablet preview too small");
        if(AdaptiveLayout.dialHeight(2400)>300)throw new AssertionError("Unbounded preview height");
        if(!AdaptiveLayout.sideBySide(720,360)||AdaptiveLayout.sideBySide(600,960))throw new AssertionError("Orientation ignored");
        if(AdaptiveLayout.dialHeight(720,360)>150)throw new AssertionError("Landscape phone preview too tall");
        System.out.println("PASS: phone, tablet portrait and tablet landscape use distinct bounded layouts.");
    }
}
