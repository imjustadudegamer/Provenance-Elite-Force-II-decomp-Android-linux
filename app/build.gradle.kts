plugins {
    id("com.android.application")
}


android {
    namespace = "com.provenanceef2"
    compileSdk = 35
    ndkVersion = "27.0.12077973"

    defaultConfig {
        // MUST match ANDROID_DATA_ROOT in src/main/cpp/cmake/platforms/android.cmake:
        applicationId = "com.provenanceef2"

        minSdk = 26           // Android 8.0, the port's platform-layer floor
        targetSdk = 35        // game data lives in the app's own OBB dir; no storage permission

        versionCode = 10021
        versionName = file("src/main/cpp/VERSION").readText().trim()
        // The name carries the commit the APK was built from, so two installs are
        // distinguishable in a manifest dump.  versionCode stays the monotone
        // release counter: moving it would re-extract the assets on every build.
        versionNameSuffix = runCatching {
            val sha = providers.exec {
                commandLine("git", "rev-parse", "--short", "HEAD")
            }.standardOutput.asText.get().trim()
            val dirty = providers.exec {
                commandLine("git", "status", "--porcelain")
            }.standardOutput.asText.get().trim()
            "+" + sha + (if (dirty.isEmpty()) "" else "-dirty")
        }.getOrElse { "+nogit" }

        externalNativeBuild {
            cmake {
                arguments += listOf(
                    "-DEF2_PLATFORM=android",
                    "-DEF2_RENDERER=gles3",
                    "-DANDROID_STL=c++_shared",
                    "-DANDROID_PLATFORM=android-26",
                    "-Wno-dev"
                )
                abiFilters += "arm64-v8a"
            }
        }
    }

    signingConfigs {
        getByName("debug") {
            storeFile = file("debug.keystore")
            storePassword = "android"
            keyAlias = "androiddebugkey"
            keyPassword = "android"
        }
        if (project.hasProperty("RELEASE_STORE_FILE")) {
            create("release") {
                storeFile = file(project.property("RELEASE_STORE_FILE") as String)
                storePassword = project.property("RELEASE_STORE_PASSWORD") as String
                keyAlias = project.property("RELEASE_KEY_ALIAS") as String
                keyPassword = project.property("RELEASE_KEY_PASSWORD") as String
            }
        }
    }

    buildTypes {
        getByName("debug") {
            signingConfig = signingConfigs.getByName("debug")
        }
        getByName("release") {
            isMinifyEnabled = false
            proguardFiles(getDefaultProguardFile("proguard-android.txt"), "proguard-rules.pro")
            signingConfig = if (project.hasProperty("RELEASE_STORE_FILE")) {
                signingConfigs.getByName("release")
            } else {
                signingConfigs.getByName("debug")
            }
        }
    }

    packaging {
        jniLibs {
            useLegacyPackaging = true
        }
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.31.5"
        }
    }

    lint {
        abortOnError = false
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_11
        targetCompatibility = JavaVersion.VERSION_11
    }
}

dependencies {
}

val libcxx16k = file("thirdparty/libcxx-16k/arm64-v8a/libc++_shared.so")

val fixLibcxxPageAlignment = tasks.register("fixLibcxxPageAlignment") {
    inputs.file(libcxx16k)
    doLast {
        if (!libcxx16k.isFile) {
            throw GradleException("missing ${libcxx16k} -- the 16 KB-aligned libc++_shared.so (see PROVENANCE.md)")
        }
        var replaced = 0
        val buildRoot = layout.buildDirectory.get().asFile
        android.buildTypes.forEach { bt ->
            val cap = bt.name.replaceFirstChar { it.uppercase() }
            listOf(
                "${buildRoot}/intermediates/merged_native_libs/${bt.name}/merge${cap}NativeLibs/out",
                "${buildRoot}/intermediates/stripped_native_libs/${bt.name}/strip${cap}DebugSymbols/out"
            ).forEach { outRoot ->
                val f = file("${outRoot}/lib/arm64-v8a/libc++_shared.so")
                if (f.isFile) {
                    f.writeBytes(libcxx16k.readBytes())
                    replaced++
                }
            }
        }
        if (replaced == 0) {
            println("fixLibcxxPageAlignment: no libc++_shared.so in the merge outputs (nothing packaged?)")
        } else {
            println("fixLibcxxPageAlignment: replaced ${replaced} libc++_shared.so with the 16 KB-aligned runtime")
        }
    }
}

tasks.whenTaskAdded {
    val t = this
    if (t.name.matches(Regex("strip.*DebugSymbols"))) {
        t.dependsOn(fixLibcxxPageAlignment)
    }
    if (t.name.matches(Regex("merge.*NativeLibs"))) {
        fixLibcxxPageAlignment.configure { mustRunAfter(t) }
    }
}
