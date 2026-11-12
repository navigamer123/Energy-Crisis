plugins {
    id("com.android.application")
}

android {
    namespace = "bg.energycrisis.game"
    compileSdk = 36

    defaultConfig {
        applicationId = "bg.energycrisis.game"
        minSdk = 24
        targetSdk = 36
        versionCode = 2
        versionName = "1.0.1"

        ndk {
            abiFilters.addAll(listOf("arm64-v8a", "armeabi-v7a", "x86_64"))
        }

        externalNativeBuild {
            cmake {
                arguments(
                    "-DANDROID_STL=c++_static",
                    "-DEC_FETCH_SFML=ON",
                    "-DEC_BUILD_GAME=ON",
                    "-DEC_BUILD_TESTS=OFF",
                    "-DCMAKE_BUILD_TYPE=Release"
                )
                cppFlags("-std=c++17")
            }
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_1_8
        targetCompatibility = JavaVersion.VERSION_1_8
    }

    val copyAssetsTree = tasks.register<Sync>("copyAssetsTree") {
        from("../../assets") {
            into("assets")
        }
        from("../../assets")
        into(layout.buildDirectory.dir("intermediates/extra_assets"))
    }

    tasks.named("preBuild") {
        dependsOn(copyAssetsTree)
    }

    sourceSets {
        getByName("main") {
            assets.srcDirs(layout.buildDirectory.dir("intermediates/extra_assets"))
        }
    }

    signingConfigs {
        create("release") {
            val keystoreFile = rootProject.file("release.keystore")
            if (keystoreFile.exists()) {
                storeFile = keystoreFile
                storePassword = System.getenv("KEYSTORE_PASSWORD") ?: "N@v!-energycrisis-GameDev-1967"
                keyAlias = System.getenv("KEY_ALIAS") ?: "energycrisis"
                keyPassword = System.getenv("KEY_PASSWORD") ?: "N@v!-energycrisis-GameDev-1967"
            } else {
                initWith(getByName("debug"))
            }
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = true
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro"
            )
            signingConfig = signingConfigs.getByName("release")
        }
        debug {
            isDebuggable = true
            isJniDebuggable = true
        }
    }

    externalNativeBuild {
        cmake {
            path("../../CMakeLists.txt")
        }
    }

    packaging {
        jniLibs {
            pickFirsts.add("**/libopenal.so")
        }
    }
}

dependencies {
    implementation("com.google.android.gms:play-services-ads:23.5.0")
    implementation("androidx.annotation:annotation:1.9.1")
}
