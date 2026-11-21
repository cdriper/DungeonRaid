// The sound table the Java shell reads through JNI: names of the 38 sounds (res/raw/<name><n>.ogg),
// how many variations each has, their volumes, and the mapping from a sound to the SoundPool ids
// Java registered for it (one is picked at random, cosmetic stream). Free functions in the original.
#pragma once

void InitialiseSoundMappings();                 // 0x000681e0: 38 empty id lists
void AddSoundMapping(int sound, int id);        // 0x00067cbc
int GetSoundMapping(int sound);                 // 0x00067ea4: a random id of the sound, -1 if none
const char* GetSoundName(int sound);            // 0x0006770c: "" for unknown sounds
int GetSoundVariations(int sound);              // 0x000678c4: 0 for unknown sounds
float GetSoundVolume(int sound);                // 0x000678f4: 0.5 for unknown sounds
int GetTotalSoundCount();                       // 0x000678dc: the sum of the variations
