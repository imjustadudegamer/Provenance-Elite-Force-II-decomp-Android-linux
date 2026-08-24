plugins {
    id("com.android.application")
}

// The APK's asset-extraction marker is versionCode: EF2Activity's .assets_version
// check re-extracts assets/base/ when it moves, so it must move whenever anything
// under assets/base/ changes.  10021 is the current pak (its global/default.cfg
// carries the port's music default).
//
// versionName comes from the engine's own VERSION file -- the single source of
// truth read here and by cmake/identity.cmake (q_version.generated.h, the startup
// banner).  Bump that file, not either build file.

android {
    namespace = "com.provenanceef2"
    compileSdk = 35
    ndkVersion = "27.0.12077973"

    defaultConfig {
        // MUST match ANDROID_DATA_ROOT in src/main/cpp/cmake/platforms/android.cmake:
        // the engine's fs_basepath/fs_homepath is /storage/emulated/0/Android/obb/<this id>.
        applicationId = "com.provenanceef2"

        minSdk = 26           // Android 8.0, the port's platform-layer floor
        targetSdk = 35        // game data lives in the app's own OBB dir; no storage permission

        versionCode = 10021
        versionName = file("src/main/cpp/VERSION").readText().trim()

        externalNativeBuild {
            cmake {
                // The device target is the only in-tree renderer: EF2_RENDERER is a
                // one-value switch (see the engine CMakeLists.txt).
                arguments += listOf(
                    "-DEF2_PLATFORM=android",
                    "-DEF2_RENDERER=gles3",
                    "-DANDROID_STL=c++_shared",
                    // Target the minSdk, not the latest API: linking against a newer API
                    // binds dynamic symbols older libc does not export, and the failure
                    // then appears at dlopen on the device rather than at link time.
                    "-DANDROID_PLATFORM=android-26",
                    // CMake "Dev Warnings" are OFF: every one comes from a file this
                    // project does not own (the NDK toolchain files and the vendored
                    // third-party CMakeLists).  Compiler warnings stay on and are held
                    // to zero.
                    "-Wno-dev"
                )
                abiFilters += "arm64-v8a"
            }
        }
    }

    signingConfigs {
        // The shared committed debug key: `adb install -r` upgrades in place instead of
        // failing on a signature mismatch that would wipe saves and imported game data.
        getByName("debug") {
            storeFile = file("debug.keystore")
            storePassword = "android"
            keyAlias = "androiddebugkey"
            keyPassword = "android"
        }
        // Release signing comes from project properties (RELEASE_STORE_FILE,
        // RELEASE_STORE_PASSWORD, RELEASE_KEY_ALIAS, RELEASE_KEY_PASSWORD).
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
            // Without a release keystore fall back to the debug key rather than
            // emitting an APK that cannot be installed at all; only the signing key
            // is the development one, the native build is still the optimised Release.
            signingConfig = if (project.hasProperty("RELEASE_STORE_FILE")) {
                signingConfigs.getByName("release")
            } else {
                signingConfigs.getByName("debug")
            }
        }
    }

    packaging {
        jniLibs {
            // The engine dlopens four of its own modules by name at run time
            // (libgame.so, libcgame.so, librenderer_gles3.so, libopenal.so -- see
            // Sys_LoadDll's __ANDROID__ branch).  That needs them EXTRACTED into the
            // app's nativeLibraryDir, which is on the dynamic linker's search path;
            // the un-extracted layout loads straight out of the APK and a bare dlopen
            // cannot see it.
            useLegacyPackaging = true
        }
    }

    externalNativeBuild {
        cmake {
            // The engine tree's own CMakeLists -- the same switch that builds the
            // Linux reference build.  Nothing is configured twice.
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.31.5"
        }
    }

    lint {
        abortOnError = false
    }

    // AGP's default Java 8 target makes javac print "[options] source value 8 is
    // obsolete" on every build; the activity code is trivially 11-compatible and
    // minSdk 26 runs it natively.
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_11
        targetCompatibility = JavaVersion.VERSION_11
    }
}

dependencies {
}

// ===========================================================================
// 16 KB PAGE ALIGNMENT.
//
// Every engine module links with -Wl,-z,max-page-size=16384
// (cmake/platforms/android.cmake), but NDK r25's own libc++_shared.so is 4 KB
// aligned and Android 15's 16 KB page-size requirement rejects it at load.  Ship
// the 16 KB-aligned runtime from NDK r27 in its place (same SONAME, superset
// DT_NEEDED -- see thirdparty/libcxx-16k/PROVENANCE.md for the symbol audit).
// AGP drops the NDK's own copy into the merged-native-libs output, so this task
// overwrites it AFTER merge<Variant>NativeLibs and BEFORE
// strip<Variant>DebugSymbols runs; strip is a no-op on the already-stripped r27
// file.  dependsOn alone is not enough (strip does not depend on this task and
// Gradle may run finalizers after dependents), hence the explicit mustRunAfter.
// ===========================================================================
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
