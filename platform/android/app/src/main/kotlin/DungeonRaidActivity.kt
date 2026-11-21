// The launcher activity: SDL's SDLActivity, which loads libSDL3.so and libmain.so and runs the
// game's C++ main() (platform/sdl/main.cpp) on its own thread. Everything else is in C++: drawing,
// touch, sound, saves, the back key, immersive full screen.
//
// Only two things are done here:
//  - Android 13+ sends back gestures to OnBackInvokedCallback instead of key events (always so
//    with targetSdk 36); they are passed on to SDL as the back key.
//  - On the first start after the update from the Kotlin shell, its settings (SharedPreferences)
//    are copied into settings.ini, the C++ host's file (same keys and values).
package com.fireflame.dungeonraid

import android.content.Context
import android.media.AudioManager
import android.os.Build
import android.os.Bundle
import android.view.KeyEvent
import android.window.OnBackInvokedDispatcher
import java.io.File
import org.libsdl.app.SDLActivity

class DungeonRaidActivity : SDLActivity() {
    override fun getLibraries(): Array<String> = arrayOf("SDL3", "main")

    override fun onCreate(savedInstanceState: Bundle?) {
        importOldSettings()
        super.onCreate(savedInstanceState)
        volumeControlStream = AudioManager.STREAM_MUSIC
        if (Build.VERSION.SDK_INT >= 33) {
            onBackInvokedDispatcher.registerOnBackInvokedCallback(OnBackInvokedDispatcher.PRIORITY_DEFAULT) {
                onNativeKeyDown(KeyEvent.KEYCODE_BACK)
                onNativeKeyUp(KeyEvent.KEYCODE_BACK)
            }
        }
    }

    private fun importOldSettings() {
        val file = File(filesDir, "settings.ini")
        if (file.exists()) return
        val prefs = getSharedPreferences("DungeonRaidPrefs", Context.MODE_PRIVATE).all
        if (prefs.isEmpty()) return
        // key=value per line; booleans as true/false, numbers in decimal (as host::Settings writes them)
        val text = prefs.entries.sortedBy { it.key }.joinToString("") { (k, v) ->
            "$k=${v.toString().replace('\n', ' ').replace('\r', ' ')}\n"
        }
        try {
            file.writeText(text)
        } catch (_: Exception) {
        }
    }
}
