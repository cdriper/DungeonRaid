// Runs inside the Emscripten module before main(): the saves on IndexedDB. SDL's pref path is
// /libsdl/DungeonRaid/ (settings, saved game, classes, scores, achievements: the same files and
// formats as on Windows and Android). /libsdl is mounted on IDBFS and loaded before main() runs
// (a run dependency holds main back); autoPersist stores every change made to it afterwards.
Module['preRun'] = Module['preRun'] || [];
Module['preRun'].push(function () {
  FS.mkdir('/libsdl');
  FS.mount(IDBFS, { autoPersist: true }, '/libsdl');
  addRunDependency('dr-saves');
  FS.syncfs(true, function (err) {
    if (err) console.warn('Dungeon Raid: cannot load the saves:', err);
    removeRunDependency('dr-saves');
  });
});
