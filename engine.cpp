// Cutline engine — C++17, compiled to WebAssembly with Emscripten.
// Owns the timeline model: sources, clips, splitting, trimming, ordering,
// per-clip effect parameters, fades, time mapping and undo/redo.
#include <emscripten/emscripten.h>
#include <vector>
#include <algorithm>
#include <cmath>

enum { P_BRIGHT, P_CONTRAST, P_SAT, P_HUE, P_TEMP, P_VIGN, P_BLUR, P_FADEIN, P_FADEOUT, P_VOLUME, P_COUNT };
static const float DEF[P_COUNT] = {0.f, 1.f, 1.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 1.f};

struct Clip { int id; int src; double in, out; float p[P_COUNT]; };

static std::vector<double> sources;
static std::vector<Clip> clips;
static std::vector<std::vector<Clip>> undoS, redoS;
static int nextId = 1;
static double qSrc = 0, qLocal = 0;

static void snap() {
  undoS.push_back(clips);
  if (undoS.size() > 300) undoS.erase(undoS.begin());
  redoS.clear();
}
static int idxOf(int id) {
  for (size_t i = 0; i < clips.size(); ++i) if (clips[i].id == id) return (int)i;
  return -1;
}
static double len(const Clip& c) { return c.out - c.in; }
static bool okI(int i) { return i >= 0 && i < (int)clips.size(); }

extern "C" {

EMSCRIPTEN_KEEPALIVE int eng_add_source(double dur) { sources.push_back(dur); return (int)sources.size() - 1; }

EMSCRIPTEN_KEEPALIVE int eng_add_clip(int s) {
  if (s < 0 || s >= (int)sources.size() || sources[s] <= 0) return -1;
  snap();
  Clip c; c.id = nextId++; c.src = s; c.in = 0; c.out = sources[s];
  for (int i = 0; i < P_COUNT; ++i) c.p[i] = DEF[i];
  clips.push_back(c);
  return c.id;
}

EMSCRIPTEN_KEEPALIVE int eng_clip_count() { return (int)clips.size(); }
EMSCRIPTEN_KEEPALIVE int eng_clip_id(int i) { return okI(i) ? clips[i].id : -1; }
EMSCRIPTEN_KEEPALIVE int eng_clip_src(int i) { return okI(i) ? clips[i].src : -1; }
EMSCRIPTEN_KEEPALIVE double eng_clip_in(int i) { return okI(i) ? clips[i].in : 0; }
EMSCRIPTEN_KEEPALIVE double eng_clip_out(int i) { return okI(i) ? clips[i].out : 0; }
EMSCRIPTEN_KEEPALIVE int eng_index_of(int id) { return idxOf(id); }

EMSCRIPTEN_KEEPALIVE double eng_clip_start(int i) {
  double t = 0;
  for (int k = 0; k < i && k < (int)clips.size(); ++k) t += len(clips[k]);
  return t;
}

EMSCRIPTEN_KEEPALIVE double eng_duration() {
  double t = 0;
  for (auto& c : clips) t += len(c);
  return t;
}

// Maps a timeline time to (clip index, source time, local time).
EMSCRIPTEN_KEEPALIVE int eng_locate(double t) {
  double s = 0;
  for (size_t i = 0; i < clips.size(); ++i) {
    double l = len(clips[i]);
    if (t >= s && t < s + l) { qLocal = t - s; qSrc = clips[i].in + qLocal; return (int)i; }
    s += l;
  }
  if (!clips.empty() && t >= s) {
    const Clip& c = clips.back();
    qLocal = std::max(0.0, len(c) - 0.001); qSrc = c.in + qLocal;
    return (int)clips.size() - 1;
  }
  return -1;
}
EMSCRIPTEN_KEEPALIVE double eng_q_src() { return qSrc; }
EMSCRIPTEN_KEEPALIVE double eng_q_local() { return qLocal; }

// Opacity of clip i at local time, from fade-in / fade-out parameters.
EMSCRIPTEN_KEEPALIVE double eng_opacity(int i, double local) {
  if (!okI(i)) return 0;
  const Clip& c = clips[i];
  double l = len(c), o = 1.0;
  if (c.p[P_FADEIN] > 0) o = std::min(o, local / c.p[P_FADEIN]);
  if (c.p[P_FADEOUT] > 0) o = std::min(o, (l - local) / c.p[P_FADEOUT]);
  return std::max(0.0, std::min(1.0, o));
}

EMSCRIPTEN_KEEPALIVE int eng_split(double t) {
  int i = eng_locate(t);
  if (i < 0) return -1;
  double local = qLocal;
  if (local < 0.05 || len(clips[i]) - local < 0.05) return -1;
  snap();
  Clip b = clips[i];
  clips[i].out = clips[i].in + local;
  clips[i].p[P_FADEOUT] = 0;
  b.in = clips[i].out; b.id = nextId++; b.p[P_FADEIN] = 0;
  clips.insert(clips.begin() + i + 1, b);
  return b.id;
}

EMSCRIPTEN_KEEPALIVE int eng_delete(int id) {
  int i = idxOf(id);
  if (i < 0) return 0;
  snap(); clips.erase(clips.begin() + i); return 1;
}

EMSCRIPTEN_KEEPALIVE int eng_duplicate(int id) {
  int i = idxOf(id);
  if (i < 0) return -1;
  snap(); Clip c = clips[i]; c.id = nextId++;
  clips.insert(clips.begin() + i + 1, c); return c.id;
}

EMSCRIPTEN_KEEPALIVE int eng_move(int id, int to) {
  int i = idxOf(id);
  if (i < 0) return 0;
  to = std::max(0, std::min(to, (int)clips.size() - 1));
  if (to == i) return 0;
  snap(); Clip c = clips[i];
  clips.erase(clips.begin() + i);
  clips.insert(clips.begin() + to, c);
  return 1;
}

// Live trim (call eng_commit() once before a drag starts for undo).
EMSCRIPTEN_KEEPALIVE void eng_trim(int id, double in, double out) {
  int i = idxOf(id);
  if (i < 0) return;
  double d = sources[clips[i].src];
  in = std::max(0.0, std::min(in, d - 0.1));
  out = std::max(in + 0.1, std::min(out, d));
  clips[i].in = in; clips[i].out = out;
}

EMSCRIPTEN_KEEPALIVE void eng_set_param(int id, int p, float v) {
  int i = idxOf(id);
  if (i >= 0 && p >= 0 && p < P_COUNT) clips[i].p[p] = v;
}
EMSCRIPTEN_KEEPALIVE float eng_get_param(int id, int p) {
  int i = idxOf(id);
  return (i >= 0 && p >= 0 && p < P_COUNT) ? clips[i].p[p] : 0.f;
}
EMSCRIPTEN_KEEPALIVE float eng_param_i(int i, int p) { return (okI(i) && p >= 0 && p < P_COUNT) ? clips[i].p[p] : 0.f; }
EMSCRIPTEN_KEEPALIVE void eng_reset_params(int id) {
  int i = idxOf(id);
  if (i < 0) return;
  snap(); for (int k = 0; k < P_COUNT; ++k) clips[i].p[k] = DEF[k];
}

EMSCRIPTEN_KEEPALIVE void eng_commit() { snap(); }
EMSCRIPTEN_KEEPALIVE int eng_undo() {
  if (undoS.empty()) return 0;
  redoS.push_back(clips); clips = undoS.back(); undoS.pop_back(); return 1;
}
EMSCRIPTEN_KEEPALIVE int eng_redo() {
  if (redoS.empty()) return 0;
  undoS.push_back(clips); clips = redoS.back(); redoS.pop_back(); return 1;
}

}
