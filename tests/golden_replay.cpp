// Golden replay test: plays the recorded games on the port and compares every snapshot with the
// state the original library produced (recorded in the old recomp repo's emulator harness).
//
//   data/scripts/<name>.json    input: seed, optional new-game setup, timing, steps (chain/menu/skill)
//   data/expected/<name>.json   one snapshot per step (turn, health, game over, menu, board, save state)
//
// The game is driven by session_driver.cpp exactly the way the old harness drove it (same boot, fake
// clock, touches, menu taps), so the save states must match byte for byte.
//
// Usage: golden_replay <data dir> [name ...]   (default: every script in <data dir>/scripts)
// Exit code 0 = every snapshot of every script matches.
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

// ------------------------------------------------------------------ session driver (session_driver.cpp)
struct Session;
extern "C" {
Session* dr_Session_New(uint32_t seed);
void dr_Session_Delete(Session* s);
void dr_Session_Frames(Session* s, int n);
void dr_Session_Setup(Session* s, const char* key, int value);
void dr_Session_NewGame(Session* s);
void dr_Session_Touch(Session* s, int kind, int row, int col);
void dr_Session_TapPoint(Session* s, double x, double y, int hold, int after);
void dr_Session_TapQuad(Session* s, int quad, int hold, int after);
int dr_Session_MenuBusy(Session* s);
int dr_Session_Turn(Session* s);
int dr_Session_Health(Session* s);
int dr_Session_GameOver(Session* s);
int dr_Session_Menu(Session* s);
int dr_Session_TileType(Session* s, int row, int col);
int dr_Session_SaveState(Session* s, uint8_t* out, int cap);
int dr_Session_Events(Session* s, char* out, int cap);
}

namespace {
// ------------------------------------------------------------------ minimal JSON reader
struct Json {
    enum Kind { Null, Bool, Number, String, Array, Object } kind = Null;
    bool b = false;
    double num = 0;
    std::string str;
    std::vector<Json> arr;
    std::vector<std::pair<std::string, Json>> obj;   // in file order

    const Json* Find(const std::string& key) const {
        for (const auto& kv : obj)
            if (kv.first == key) return &kv.second;
        return nullptr;
    }
    const Json& At(const std::string& key) const {
        const Json* j = Find(key);
        if (!j) throw std::runtime_error("missing key " + key);
        return *j;
    }
    int Int() const { return static_cast<int>(num); }
};

class Parser {
public:
    explicit Parser(const std::string& s) : s_(s) {}
    Json Parse() {
        Json j = Value();
        Ws();
        if (p_ != s_.size()) Fail("trailing data");
        return j;
    }

private:
    const std::string& s_;
    size_t p_ = 0;

    [[noreturn]] void Fail(const char* what) {
        throw std::runtime_error(std::string("JSON: ") + what + " at offset " + std::to_string(p_));
    }
    void Ws() {
        while (p_ < s_.size() && (s_[p_] == ' ' || s_[p_] == '\n' || s_[p_] == '\r' || s_[p_] == '\t')) ++p_;
    }
    char Peek() {
        Ws();
        if (p_ >= s_.size()) Fail("unexpected end");
        return s_[p_];
    }
    void Expect(char c) {
        if (Peek() != c) Fail("unexpected character");
        ++p_;
    }
    bool Literal(const char* lit) {
        size_t n = std::char_traits<char>::length(lit);
        if (s_.compare(p_, n, lit) != 0) return false;
        p_ += n;
        return true;
    }
    std::string Str() {
        Expect('"');
        std::string out;
        while (p_ < s_.size() && s_[p_] != '"') {
            char c = s_[p_++];
            if (c == '\\') {
                if (p_ >= s_.size()) Fail("bad escape");
                char e = s_[p_++];
                switch (e) {
                    case 'n': out += '\n'; break;
                    case 't': out += '\t'; break;
                    case 'r': out += '\r'; break;
                    case 'u': p_ += 4; out += '?'; break;   // not used by the data files
                    default: out += e; break;
                }
            } else {
                out += c;
            }
        }
        Expect('"');
        return out;
    }
    Json Value() {
        Json j;
        char c = Peek();
        if (c == '{') {
            ++p_;
            j.kind = Json::Object;
            if (Peek() == '}') { ++p_; return j; }
            for (;;) {
                std::string k = Str();
                Expect(':');
                j.obj.emplace_back(std::move(k), Value());
                if (Peek() == ',') { ++p_; continue; }
                Expect('}');
                return j;
            }
        }
        if (c == '[') {
            ++p_;
            j.kind = Json::Array;
            if (Peek() == ']') { ++p_; return j; }
            for (;;) {
                j.arr.push_back(Value());
                if (Peek() == ',') { ++p_; continue; }
                Expect(']');
                return j;
            }
        }
        if (c == '"') {
            j.kind = Json::String;
            j.str = Str();
            return j;
        }
        if (Literal("true")) { j.kind = Json::Bool; j.b = true; return j; }
        if (Literal("false")) { j.kind = Json::Bool; return j; }
        if (Literal("null")) return j;
        size_t start = p_;
        while (p_ < s_.size() && (std::isdigit(static_cast<unsigned char>(s_[p_])) || s_[p_] == '-' ||
                                  s_[p_] == '+' || s_[p_] == '.' || s_[p_] == 'e' || s_[p_] == 'E'))
            ++p_;
        if (start == p_) Fail("bad value");
        j.kind = Json::Number;
        j.num = std::stod(s_.substr(start, p_ - start));
        return j;
    }
};

Json LoadJson(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("cannot open " + path.string());
    std::stringstream ss;
    ss << in.rdbuf();
    std::string text = ss.str();
    return Parser(text).Parse();
}

// ------------------------------------------------------------------ the replay (old harness: script.py)
// Constants of the old harness (session.py).
constexpr double kTopButtons[4] = {0.1015625, 0.2859375, 0.4703125, 0.6546875};   // skill buttons x
constexpr double kTopButtonY = 0.1015625;
constexpr int kFoldoutConfirmQuad = 7;

struct Timing {
    int menuFrames = 30, newGameSettle = 5, framesPerStep = 3, chainSettle = 120, menuSettle = 60,
        skillSettle = 90;
};

struct Snapshot {
    int turn = 0, health = 0, menu = -1;
    bool gameOver = false;
    std::vector<std::string> board;
    std::string save;   // hex
};

char TileLetter(int type) {
    switch (type) {
        case 0: return 'P';   // potion
        case 1: return 'S';   // skull
        case 2: return 'C';   // coin
        case 3: return 'D';   // shield
        case 4: return 'W';   // sword
        case 6: return '*';
        case 7: return '.';
        default: return '?';
    }
}

// Same order as script.snapshot(): the save state last (SerialiseState can queue an achievement).
Snapshot Take(Session* s) {
    Snapshot snap;
    snap.turn = dr_Session_Turn(s);
    snap.health = dr_Session_Health(s);
    snap.gameOver = dr_Session_GameOver(s) != 0;
    snap.menu = dr_Session_Menu(s);
    for (int r = 0; r < 6; ++r) {
        std::string row;
        for (int c = 0; c < 6; ++c) row += TileLetter(dr_Session_TileType(s, r, c));
        snap.board.push_back(row);
    }
    std::vector<uint8_t> buf(0x10000);
    int n = dr_Session_SaveState(s, buf.data(), static_cast<int>(buf.size()));
    static const char* kHex = "0123456789abcdef";
    for (int i = 0; i < n; ++i) {
        snap.save += kHex[buf[i] >> 4];
        snap.save += kHex[buf[i] & 15];
    }
    return snap;
}

Snapshot FromJson(const Json& j) {
    Snapshot snap;
    snap.turn = j.At("turn").Int();
    snap.health = j.At("health").Int();
    snap.gameOver = j.At("game_over").b;
    const Json& m = j.At("menu");
    snap.menu = m.kind == Json::Null ? -1 : m.At("type").Int();
    for (const Json& row : j.At("board").arr) snap.board.push_back(row.str);
    snap.save = j.At("save").str;
    return snap;
}

// Empty when equal, else the first difference.
std::string Compare(const Snapshot& got, const Snapshot& want) {
    auto num = [](const char* f, int g, int w) {
        return std::string(f) + ": port " + std::to_string(g) + ", expected " + std::to_string(w);
    };
    if (got.turn != want.turn) return num("turn", got.turn, want.turn);
    if (got.health != want.health) return num("health", got.health, want.health);
    if (got.gameOver != want.gameOver) return num("game_over", got.gameOver, want.gameOver);
    if (got.menu != want.menu) return num("menu", got.menu, want.menu);
    for (size_t r = 0; r < want.board.size(); ++r)
        if (r >= got.board.size() || got.board[r] != want.board[r])
            return "board row " + std::to_string(r) + ": port " + (r < got.board.size() ? got.board[r] : "") +
                   ", expected " + want.board[r];
    if (got.save != want.save) {
        size_t i = 0;
        while (i < got.save.size() && i < want.save.size() && got.save[i] == want.save[i]) ++i;
        return "save state: size port " + std::to_string(got.save.size() / 2) + ", expected " +
               std::to_string(want.save.size() / 2) + "; first differing byte " + std::to_string(i / 2);
    }
    return {};
}

void PlayChain(Session* s, const Json& path, const Timing& t) {
    dr_Session_Touch(s, 0, path.arr[0].arr[0].Int(), path.arr[0].arr[1].Int());
    dr_Session_Frames(s, t.framesPerStep);
    for (size_t i = 1; i < path.arr.size(); ++i) {
        dr_Session_Touch(s, 1, path.arr[i].arr[0].Int(), path.arr[i].arr[1].Int());
        dr_Session_Frames(s, t.framesPerStep);
    }
    dr_Session_Touch(s, 2, 0, 0);
    dr_Session_Frames(s, t.chainSettle);
}

// Same sequence as the old harness's menu_choose: wait until the menu takes a new press, tap the
// option quads and the confirm quad, wait until the menu has closed, settle.
void MenuChoose(Session* s, const Json& quads, const Timing& t) {
    constexpr int kMaxWait = 300;
    for (int i = 0; i < kMaxWait && dr_Session_MenuBusy(s) != 0; ++i) dr_Session_Frames(s, 1);
    for (const Json& q : quads.arr) dr_Session_TapQuad(s, q.Int(), 2, 2);
    dr_Session_TapQuad(s, kFoldoutConfirmQuad, 2, 2);
    for (int i = 0; i < kMaxWait && dr_Session_Menu(s) >= 0; ++i) dr_Session_Frames(s, 1);
    dr_Session_Frames(s, t.menuSettle);
}

void ApplyStep(Session* s, const Json& step, const Timing& t) {
    if (const Json* chain = step.Find("chain")) {
        PlayChain(s, *chain, t);
    } else if (const Json* menu = step.Find("menu")) {
        MenuChoose(s, *menu, t);
    } else if (const Json* skill = step.Find("skill")) {
        dr_Session_TapPoint(s, kTopButtons[skill->Int()], kTopButtonY, 2, t.skillSettle);
    } else {
        throw std::runtime_error("unknown step kind");
    }
}

Timing ReadTiming(const Json& script) {
    Timing t;
    const Json* j = script.Find("timing");
    if (!j) return t;
    const std::map<std::string, int*> fields = {
        {"menu_frames", &t.menuFrames},     {"new_game_settle", &t.newGameSettle},
        {"frames_per_step", &t.framesPerStep}, {"chain_settle", &t.chainSettle},
        {"menu_settle", &t.menuSettle},     {"skill_settle", &t.skillSettle}};
    for (const auto& kv : j->obj) {
        auto it = fields.find(kv.first);
        if (it == fields.end()) throw std::runtime_error("unknown timing key " + kv.first);
        *it->second = kv.second.Int();
    }
    return t;
}

// Empty when every snapshot matches, else what went wrong.
std::string RunScript(const std::filesystem::path& dataDir, const std::string& name) {
    Json script = LoadJson(dataDir / "scripts" / (name + ".json"));
    Json expected = LoadJson(dataDir / "expected" / (name + ".json"));
    const std::vector<Json>& snaps = expected.At("snapshots").arr;
    const std::vector<Json>& steps = script.At("steps").arr;
    if (snaps.size() != steps.size() + 1)
        return "expected file has " + std::to_string(snaps.size()) + " snapshots for " +
               std::to_string(steps.size()) + " steps";

    Timing t = ReadTiming(script);
    Session* s = dr_Session_New(static_cast<uint32_t>(script.At("seed").num));
    std::unique_ptr<Session, void (*)(Session*)> guard(s, dr_Session_Delete);
    dr_Session_Frames(s, t.menuFrames);
    if (const Json* setup = script.Find("setup"))
        for (const auto& kv : setup->obj) dr_Session_Setup(s, kv.first.c_str(), kv.second.Int());
    dr_Session_NewGame(s);
    dr_Session_Frames(s, t.newGameSettle);

    for (size_t i = 0; i <= steps.size(); ++i) {
        if (i > 0) ApplyStep(s, steps[i - 1], t);
        std::string diff = Compare(Take(s), FromJson(snaps[i]));
        if (!diff.empty()) return "step " + std::to_string(i) + ": " + diff;
    }
    int n = dr_Session_Events(s, nullptr, 0);
    if (n > 0) {
        std::string ev(static_cast<size_t>(n) + 1, '\0');
        dr_Session_Events(s, ev.data(), n + 1);
        ev.resize(static_cast<size_t>(n));
        return "unsupported hook calls on the port:\n" + ev;
    }
    return {};
}
}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: golden_replay <data dir> [name ...]\n");
        return 2;
    }
    std::filesystem::path dataDir = argv[1];
    std::vector<std::string> names;
    for (int i = 2; i < argc; ++i) names.emplace_back(argv[i]);
    if (names.empty()) {
        for (const auto& e : std::filesystem::directory_iterator(dataDir / "scripts"))
            if (e.path().extension() == ".json") names.push_back(e.path().stem().string());
        std::sort(names.begin(), names.end());
    }
    if (names.empty()) {
        std::fprintf(stderr, "no scripts in %s\n", (dataDir / "scripts").string().c_str());
        return 2;
    }

    int failed = 0;
    for (const std::string& name : names) {
        std::string err;
        try {
            err = RunScript(dataDir, name);
        } catch (const std::exception& e) {
            err = e.what();
        }
        std::printf("%-24s %s\n", name.c_str(), err.empty() ? "OK" : ("FAIL  " + err).c_str());
        if (!err.empty()) ++failed;
    }
    std::printf("%zu scripts, %d failed\n", names.size(), failed);
    return failed == 0 ? 0 : 1;
}
