// The app: SDL's Java sources (SDLActivity, from third_party/SDL3/android-java) + a small Kotlin
// launcher (flat in src/main/kotlin/) + the native libraries built with the NDK from the repo's
// CMakeLists.txt: libSDL3.so and libmain.so (the core, the shared host layer, platform/sdl).
// Needs the third-party sources: cmake -P third_party/fetch.cmake (from the repo root).
//
// The game's textures, sounds and fonts are copied from ../../assets into the APK's assets at build
// time (into build/app/generated/sdl/, never into src/), with assets.txt listing them; the app copies
// them out to its files folder on the first start of each build (platform/sdl/main.cpp).
//
// All outputs go under the root build/ folder (build/app, build/cxx), not app/build and app/.cxx.
import java.security.MessageDigest

plugins {
    id("com.android.application")
}

layout.buildDirectory.set(rootProject.layout.buildDirectory.dir("app"))

val repoDir = rootProject.file("../..")
val assetsDir = repoDir.resolve("assets")

// The port's version: the repo's VERSION file (as for CMake; versionName shows it whole, a suffix
// such as "rc1" included). versionCode, which must grow for an update to install over an older APK,
// from its leading numbers: major * 10000 + minor * 100 + patch (0.1 -> 100, 1.2.3 -> 10203,
// 0.2rc1 -> 200, the same as 0.2).
val drVersion = repoDir.resolve("VERSION").readText().trim()
val drVersionNumber = Regex("^[0-9]+(\\.[0-9]+)*").find(drVersion)?.value
    ?: error("VERSION must start with a number (e.g. 0.2 or 0.2rc1), not '$drVersion'")
val drVersionCode = drVersionNumber.split(".").map { it.toInt() }.let { p ->
    p.getOrElse(0) { 0 } * 10000 + p.getOrElse(1) { 0 } * 100 + p.getOrElse(2) { 0 }
}
val generatedRes = layout.buildDirectory.dir("generated/sdl/res")
val generatedAssets = layout.buildDirectory.dir("generated/sdl/assets")

val copyIcons = tasks.register<Sync>("copyIcons") {
    description = "Copies the app icon from assets/icons into Android resources"
    for (density in listOf("ldpi", "mdpi", "hdpi", "xhdpi")) {
        from(assetsDir.resolve("icons/$density")) { include("icon.png"); into("mipmap-$density") }
    }
    into(generatedRes)
}

val copyGameAssets = tasks.register<Sync>("copyGameAssets") {
    description = "Copies what the game loads from assets/ into the APK's assets, plus the list assets.txt"
    from(assetsDir) {
        include("textures/*.png", "fonts/*.fnt", "sounds/*.ogg")
        exclude("textures/splash.png")
    }
    into(generatedAssets)
    doLast {
        // assets.txt: a stamp (hash of every path and its content), then the paths, one per line
        val root = destinationDir
        val files = root.walkTopDown().filter { it.isFile && it.name != "assets.txt" }
            .map { it.relativeTo(root).invariantSeparatorsPath }.sorted().toList()
        val md = MessageDigest.getInstance("SHA-256")
        for (path in files) {
            md.update(path.toByteArray())
            md.update(root.resolve(path).readBytes())
        }
        val stamp = md.digest().joinToString("") { "%02x".format(it) }
        root.resolve("assets.txt").writeText((listOf(stamp) + files).joinToString("\n", postfix = "\n"))
    }
}

android {
    namespace = "com.fireflame.dungeonraid"
    compileSdk = 36
    buildToolsVersion = "36.1.0"
    ndkVersion = "30.0.16248370"

    defaultConfig {
        // A new id, so the port installs next to the original (com.fireflame.dungeonraid).
        applicationId = "com.fireflame.dungeonraid.recomp"
        minSdk = 21
        targetSdk = 36
        versionCode = drVersionCode
        versionName = drVersion
        ndk {
            abiFilters += listOf("armeabi-v7a", "arm64-v8a", "x86_64")
        }
        externalNativeBuild {
            cmake {
                arguments += listOf("-DANDROID_STL=c++_static", "-DDR_BUILD_TESTS=OFF")
                targets += listOf("main", "SDL3-shared")
            }
        }
    }

    externalNativeBuild {
        cmake {
            path = repoDir.resolve("CMakeLists.txt")
            version = "3.31.5"
            buildStagingDirectory = rootProject.layout.buildDirectory.dir("cxx").get().asFile
        }
    }

    sourceSets {
        getByName("main") {
            // (plain directories: AGP 9 refuses Provider instances here; preBuild runs the copies)
            java.srcDir(repoDir.resolve("third_party/SDL3/android-java"))
            res.srcDir(generatedRes.get().asFile)
            assets.srcDir(generatedAssets.get().asFile)
        }
    }

    androidResources {
        // Ogg and PNG are stored uncompressed anyway; .fnt and assets.txt are tiny.
        noCompress += listOf("ogg", "png")
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    buildTypes {
        getByName("release") {
            isMinifyEnabled = false
        }
    }
}

tasks.named("preBuild") {
    dependsOn(copyIcons, copyGameAssets)
}
