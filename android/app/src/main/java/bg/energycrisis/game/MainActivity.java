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
import com.google.android.gms.ads.rewarded.RewardItem;
import com.google.android.gms.ads.rewarded.RewardedAd;
import com.google.android.gms.ads.rewarded.RewardedAdLoadCallback;

public class MainActivity extends NativeActivity {
    private static final String TAG = "EnergyCrisisAds";
    
    // Official AdMob Rewarded Ad Unit ID for Energy Crisis (Premium Level single tier)
    public static final String AD_UNIT_ID = "ca-app-pub-8879039481537676/4813432180";

    private static MainActivity sInstance;
    private RewardedAd mRewardedAd;
    private boolean mIsLoading = false;
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

        // Initialize Google Mobile Ads SDK
        MobileAds.initialize(this, initializationStatus -> {
            Log.i(TAG, "Google Mobile Ads SDK initialized successfully.");
            loadRewardedAd();
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
     */
    public void loadRewardedAd() {
        if (mRewardedAd != null || mIsLoading) {
            return;
        }

        mIsLoading = true;
        Log.i(TAG, "Requesting rewarded ad load: " + AD_UNIT_ID);
        AdRequest adRequest = new AdRequest.Builder().build();

        RewardedAd.load(this, AD_UNIT_ID, adRequest, new RewardedAdLoadCallback() {
            @Override
            public void onAdFailedToLoad(@NonNull LoadAdError loadAdError) {
                Log.w(TAG, "Failed to load rewarded ad: " + loadAdError.getMessage() + " (code: " + loadAdError.getCode() + ")");
                mRewardedAd = null;
                mIsLoading = false;
            }

            @Override
            public void onAdLoaded(@NonNull RewardedAd rewardedAd) {
                Log.i(TAG, "Rewarded ad loaded successfully.");
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
                        loadRewardedAd();
                        nativeOnAdFailed();
                    }

                    @Override
                    public void onAdDismissedFullScreenContent() {
                        Log.i(TAG, "Rewarded ad dismissed by user.");
                        mRewardedAd = null;
                        loadRewardedAd(); // Preload next ad
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
                Toast.makeText(MainActivity.this, "Ad is loading, please try again in a moment...", Toast.LENGTH_SHORT).show();
                loadRewardedAd();
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

