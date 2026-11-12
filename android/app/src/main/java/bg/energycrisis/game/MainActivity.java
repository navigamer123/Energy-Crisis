package bg.energycrisis.game;

import android.app.NativeActivity;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.widget.Toast;
import androidx.annotation.NonNull;

import com.google.android.gms.ads.AdError;
import com.google.android.gms.ads.AdRequest;
import com.google.android.gms.ads.FullScreenContentCallback;
import com.google.android.gms.ads.LoadAdError;
import com.google.android.gms.ads.MobileAds;
import com.google.android.gms.ads.OnUserEarnedRewardListener;
import com.google.android.gms.ads.RequestConfiguration;
import com.google.android.gms.ads.rewarded.RewardItem;
import com.google.android.gms.ads.rewarded.RewardedAd;
import com.google.android.gms.ads.rewarded.RewardedAdLoadCallback;

import java.util.Arrays;
import java.util.List;

public class MainActivity extends NativeActivity {
    private static final String TAG = "EnergyCrisisAds";
    
    // Official AdMob Rewarded Ad Unit ID for Energy Crisis (Premium Level single tier)
    public static final String PROD_AD_UNIT_ID = "ca-app-pub-8879039481537676/4813432180";
    // Google official sample Rewarded Ad Unit ID (guaranteed 100% fill for testing)
    public static final String TEST_AD_UNIT_ID = "ca-app-pub-3940256099942544/5224354917";

    private static MainActivity sInstance;
    private RewardedAd mRewardedAd;
    private boolean mIsLoading = false;
    private boolean mUsingFallback = false;
    private final Handler mMainHandler = new Handler(Looper.getMainLooper());

    // Native C++ JNI callbacks implemented in UI_ads.cpp
    public static native void nativeOnRewardEarned();
    public static native void nativeOnAdClosed();
    public static native void nativeOnAdFailed();
    public static native void nativeOnAdNotReady();

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        sInstance = this;

        // Register test devices so developer phone always receives test fill safely
        List<String> testDeviceIds = Arrays.asList(
            "2A217D25319830DA4881BFFBC4ABE250",
            AdRequest.DEVICE_ID_EMULATOR
        );
        RequestConfiguration configuration = new RequestConfiguration.Builder()
            .setTestDeviceIds(testDeviceIds)
            .build();
        MobileAds.setRequestConfiguration(configuration);

        // Initialize Google Mobile Ads SDK
        MobileAds.initialize(this, initializationStatus -> {
            Log.i(TAG, "Google Mobile Ads SDK initialized successfully.");
            loadRewardedAd(false);
        });
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        if (sInstance == this) {
            sInstance = null;
        }
    }

    public static MainActivity getInstance() {
        return sInstance;
    }

    /**
     * Loads a Rewarded Ad from Google AdMob.
     * Tries production unit first; if NO_FILL occurs (new account / unverified),
     * falls back to Google's official test ad unit so the user can test immediately.
     */
    public void loadRewardedAd(boolean useFallback) {
        if (mRewardedAd != null || mIsLoading) {
            return;
        }

        mIsLoading = true;
        mUsingFallback = useFallback;
        final String adUnitToLoad = useFallback ? TEST_AD_UNIT_ID : PROD_AD_UNIT_ID;

        Log.i(TAG, "Requesting rewarded ad load: " + adUnitToLoad + " (fallback=" + useFallback + ")");
        AdRequest adRequest = new AdRequest.Builder().build();

        RewardedAd.load(this, adUnitToLoad, adRequest, new RewardedAdLoadCallback() {
            @Override
            public void onAdFailedToLoad(@NonNull LoadAdError loadAdError) {
                Log.w(TAG, "Failed to load rewarded ad (" + adUnitToLoad + "): " + 
                      loadAdError.getMessage() + " (code: " + loadAdError.getCode() + ")");
                mRewardedAd = null;
                mIsLoading = false;

                // If production ad unit returned NO_FILL (code 3), fallback to Google test unit
                if (!useFallback && loadAdError.getCode() == AdRequest.ERROR_CODE_NO_FILL) {
                    Log.i(TAG, "Production unit returned NO_FILL. Falling back to test ad unit.");
                    loadRewardedAd(true);
                }
            }

            @Override
            public void onAdLoaded(@NonNull RewardedAd rewardedAd) {
                Log.i(TAG, "Rewarded ad loaded successfully from " + (useFallback ? "test unit" : "production unit"));
                mRewardedAd = rewardedAd;
                mIsLoading = false;
            }
        });
    }

    public boolean isAdReady() {
        return mRewardedAd != null;
    }

    /**
     * Displays the Rewarded Ad on the Android UI thread.
     */
    public void showRewardedAd() {
        mMainHandler.post(() -> {
            if (mRewardedAd != null) {
                mRewardedAd.setFullScreenContentCallback(new FullScreenContentCallback() {
                    @Override
                    public void onAdShowedFullScreenContent() {
                        Log.i(TAG, "Rewarded ad displayed on screen.");
                    }

                    @Override
                    public void onAdFailedToShowFullScreenContent(@NonNull AdError adError) {
                        Log.e(TAG, "Rewarded ad failed to display: " + adError.getMessage());
                        mRewardedAd = null;
                        loadRewardedAd(false);
                        nativeOnAdFailed();
                    }

                    @Override
                    public void onAdDismissedFullScreenContent() {
                        Log.i(TAG, "Rewarded ad dismissed by user.");
                        mRewardedAd = null;
                        loadRewardedAd(false); // Preload next ad
                        nativeOnAdClosed();
                    }
                });

                mRewardedAd.show(MainActivity.this, new OnUserEarnedRewardListener() {
                    @Override
                    public void onUserEarnedReward(@NonNull RewardItem rewardItem) {
                        Log.i(TAG, "User earned reward! Type: " + rewardItem.getType() + ", Amount: " + rewardItem.getAmount());
                        nativeOnRewardEarned();
                    }
                });
            } else {
                Log.w(TAG, "Rewarded ad not ready when requested.");
                Toast.makeText(MainActivity.this, "Loading ad, please try again in 2 seconds...", Toast.LENGTH_SHORT).show();
                loadRewardedAd(false);
                nativeOnAdNotReady();
            }
        });
    }

    // Static hooks callable directly from C++ via JNI (UI_ads.cpp)
    public static void showAdFromNative() {
        if (sInstance != null) {
            sInstance.showRewardedAd();
        } else {
            Log.e(TAG, "showAdFromNative: MainActivity instance is null!");
        }
    }

    public static boolean isAdReadyFromNative() {
        return sInstance != null && sInstance.isAdReady();
    }
}


