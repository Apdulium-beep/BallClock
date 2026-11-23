package local.ballclock.controller;
public final class WifiLeasePolicyTest {
    public static void main(String[] args){
        if(WifiLeasePolicy.onBackground(true,false,false,false)!=WifiLeasePolicy.KEEP)throw new AssertionError("Rotation disconnected Wi-Fi");
        if(WifiLeasePolicy.onBackground(false,false,true,true)!=WifiLeasePolicy.WAIT_FOR_PICKER)throw new AssertionError("System picker request cancelled");
        if(WifiLeasePolicy.onBackground(false,false,false,true)!=WifiLeasePolicy.RELEASE)throw new AssertionError("Home retained acquired lease");
        if(WifiLeasePolicy.onBackground(false,true,true,false)!=WifiLeasePolicy.RELEASE)throw new AssertionError("Exit kept pending request");
        if(WifiLeasePolicy.onBackground(true,true,true,false)!=WifiLeasePolicy.RELEASE)throw new AssertionError("Finish must override retain");
        for(int edit=0;edit<100;edit++)if(WifiLeasePolicy.onBackground(false,false,false,false)!=WifiLeasePolicy.KEEP)throw new AssertionError("Settings/dialog event disconnected Wi-Fi");
        for(int version:new int[]{26,28,29,30,31,32})if(!WifiLeasePolicy.permission(version).endsWith("ACCESS_FINE_LOCATION"))throw new AssertionError("Wrong legacy permission");
        for(int version:new int[]{33,34,35,36})if(!WifiLeasePolicy.permission(version).endsWith("NEARBY_WIFI_DEVICES"))throw new AssertionError("Wrong nearby permission");
        System.out.println("PASS: Wi-Fi lease release, rotation retention, system-picker protection and Android permission policy.");
    }
}
