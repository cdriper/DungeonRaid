// App: the object the Java shell holds (through SWIG). In the original it is one word, the Game
// pointer; init creates the Game with the view size, done deletes it. resize and pause are empty.
#pragma once

class Game;

class App {
public:
    App() = default;                            // 0x000243b0
    ~App() = default;                           // 0x000243c0 (does not delete the Game; done does)
    App(const App&) = delete;
    App& operator=(const App&) = delete;

    static int resize(int width, int /*height*/) { return width; }   // 0x000243c8 (returns its first argument)
    static void pause() {}                      // 0x000243cc
    void render();                              // 0x000243d0: Game::Render
    void process();                             // 0x000243dc: Game::Process
    void done();                                // 0x000243e8: delete the Game
    void init(float viewWidth, float viewHeight);   // 0x00024404: new Game(viewWidth, viewHeight)

    Game* m_game = nullptr;                     // 0x0
};
