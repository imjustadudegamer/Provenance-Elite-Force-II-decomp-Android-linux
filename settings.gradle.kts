// ProvenanceEF2 -- Star Trek: Elite Force II retail-exact decomp port, Android APK.
//
// One :app module whose externalNativeBuild drives the engine's own CMakeLists.txt
// with the NDK toolchain. There is no second CMake project: the same
// EF2_PLATFORM / EF2_RENDERER switch that builds the Linux reference build builds
// the device target, so the two cannot drift.

pluginManagement {
    repositories {
        google()
        mavenCentral()
        gradlePluginPortal()
    }
}

dependencyResolutionManagement {
    repositoriesMode.set(RepositoriesMode.FAIL_ON_PROJECT_REPOS)
    repositories {
        google()
        mavenCentral()
    }
}

rootProject.name = "provenanceef2"
include(":app")
