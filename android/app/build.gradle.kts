plugins {
    id("com.android.application")
}

android {
    namespace = "bg.energycrisis.game"
    compileSdk = 34

    defaultConfig {
        applicationId = "bg.energycrisis.game"
        minSdk = 24
        targetSdk = 34
        versionCode = 1
        versionName = "1.0.0"

        ndk {
            abiFilters.addAll(listOf("arm64-v8a", "armeabi-v7a", "x86_64"))
        }

        externalNativeBuild {
            cmake {
                arguments(
                    "-DANDROID_STL=c++_shared",
                    "-DEC_FETCH_SFML=ON",
                    "-DEC_BUILD_GAME=ON",
                    "-DEC_BUILD_TESTS=OFF",
                    "-DCMAKE_BUILD_TYPE=Release"
                )
                cppFlags("-std=c++17")
            }
        }
    }

    sourceSets {
        getByName("main") {
            assets.srcDirs("../../assets")
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro"
            )
            signingConfig = signingConfigs.getByName("debug")
        }
        debug {
            isDebuggable = true
            isJniDebuggable = true
        }
    }

    externalNativeBuild {
        cmake {
            path("../../CMakeLists.txt")
            version = "3.30.5"
        }
    }

    packaging {
        jniLibs {
            pickFirsts.add("**/libc++_shared.so")
            pickFirsts.add("**/libopenal.so")
        }
    }
}

dependencies {
    // Pure native C++ NativeActivity app - no Java/AndroidX runtime dependencies needed
}
