package com.github.eka2l1.applist;

import com.google.gson.annotations.SerializedName;

public class AppLaunchInfo {
    @SerializedName("AppUid")
    public long appUid;

    @SerializedName("AppName")
    public String appName;

    @SerializedName("DeviceCode")
    public String deviceCode;

    public AppLaunchInfo(long appUid, String appName, String deviceCode) {
        this.appUid = appUid;
        this.appName = appName;
        this.deviceCode = deviceCode;
    }
}
