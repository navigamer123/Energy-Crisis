# Energy Crisis Proguard rules
-keep class android.app.NativeActivity { *; }
-keepclassmembers class * extends android.app.NativeActivity { *; }

-keep class bg.energycrisis.game.MainActivity { *; }
-keepclassmembers class bg.energycrisis.game.MainActivity {
    public static native <methods>;
    public static void showAdFromNative();
    public static boolean isAdReadyFromNative();
}

# Keep Google Play Services Ads classes
-keep class com.google.android.gms.ads.** { *; }
-keep interface com.google.android.gms.ads.** { *; }
