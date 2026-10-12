// Cutline engine v2 â€” C++17, compiled to WebAssembly with Emscripten.
// Owns the whole project model:
//   * media sources (durations)
//   * main video track: clips with colour / effect / transform / speed / transition params
//   * text track: titles & subtitles with style params and UTF-8 text
//   * audio track: music / voice-over items placed freely on two lanes
//   * timeline markers
//   * snapping, time mapping, transitions, fades, undo/redo of the full state
#include <emscripten/emscripten.h>
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>

// ---------- Clip parameters ----------
enum {
  P_BRIGHT, P_CONTRAST, P_SAT, P_HUE, P_TEMP, P_TINT, P_VIGN, P_BLUR, P_SHARP, P_GRAIN, P_PIXEL, P_ABERR, P_SPLIT,
  P_FADEIN, P_FADEOUT, P_VOLUME, P_SPEED, P_SCALE, P_POSX, P_POSY, P_ROT, P_FLIP, P_KB, P_FIT, P_BGBLUR,
  P_TRTYPE, P_TRDUR, P_OPACITY, P_COUNT
};
static const float DEF[P_COUNT] = {
  0.f, 1.f, 1.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f,
  0.f, 0.f, 1.f, 1.f, 1.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f,
  0.f, 0.5f, 1.f
};

// ---------- Text overlay parameters ----------
enum { O_X, O_Y, O_SIZE, O_COLOR, O_BG, O_BGCOLOR, O_BGOP, O_FONT, O_ANIM, O_ALIGN, O_OUTLINE, O_BOLD, O_LANE, O_SHADOW, O_COUNT };
static const double ODEF[O_COUNT] = { 0.5, 0.85, 0.07, 16777215.0, 0, 0, 0.6, 0, 1, 1, 1, 1, 0, 1 };

// ---------- Audio item parameters ----------
enum { A_VOL, A_FIN, A_FOUT, A_LANE, A_COUNT };
static const double ADEF[A_COUNT] = { 1, 0, 0, 0 };

struct Clip { int id; int src; double in, out; float p[P_COUNT]; };
struct Ov   { int id; double start, dur; double q[O_COUNT]; std::string text; };
struct Au   { int id; int src; double start, in, out; double a[A_COUNT]; };
struct State {
  std::vector<Clip> clips;
  std::vector<Ov> ovs;
  std::vector<Au> aus;
  std::vector<double> marks;
};

static std::vector<double> sources;
static State S;
static std::vector<State> undoS, redoS;
static int nextId = 1;
static double qSrc = 0, qLocal = 0;
static std::string strBuf;

static void snap() {
  undoS.push_back(S);
  if (undoS.size() > 300) undoS.erase(undoS.begin());
  redoS.clear();
}
template <class T> static int findId(const std::vector<T>& v, int id) {
  for (size_t i = 0; i < v.size(); ++i) if (v[i].id == id) return (int)i;
  return -1;
}
static double spd(const Clip& c) { return std::max(0.05f, c.p[P_SPEED]); }
static double len(const Clip& c) { return (c.out - c.in) / spd(c); }
static bool okI(int i) { return i >= 0 && i < (int)S.clips.size(); }
static bool okO(int i) { return i >= 0 && i < (int)S.ovs.size(); }
static bool okA(int i) { return i >= 0 && i < (int)S.aus.size(); }
static double srcDur(int s) { return (s >= 0 && s < (int)sources.size()) ? sources[s] : 0; }
static double clampd(double v, double a, double b) { return std::max(a, std::min(b, v)); }

static Clip makeClip(int s, double length) {
  Clip c; c.id = nextId++; c.src = s; c.in = 0;
  c.out = (length > 0) ? std::min(length, sources[s]) : sources[s];
  for (int i = 0; i < P_COUNT; ++i) c.p[i] = DEF[i];
  return c;
}

extern "C" {

// ================= Sources / project =================
EMSCRIPTEN_KEEPALIVE int eng_add_source(double dur) { sources.push_back(dur); return (int)sources.size() - 1; }
EMSCRIPTEN_KEEPALIVE double eng_source_dur(int s) { return srcDur(s); }
EMSCRIPTEN_KEEPALIVE void eng_clear_all() {
  sources.clear(); S = State(); undoS.clear(); redoS.clear(); nextId = 1;
}

// ================= Video track =================
// length <= 0 -> whole source
EMSCRIPTEN_KEEPALIVE int eng_add_clip(int s, double length) {
  if (s < 0 || s >= (int)sources.size() || sources[s] <= 0) return -1;
  snap();
  Clip c = makeClip(s, length);
  S.clips.push_back(c);
  return c.id;
}
// Raw loader (no undo entry) used by project open.
EMSCRIPTEN_KEEPALIVE int eng_load_clip(int s, double in, double out) {
  if (s < 0 || s >= (int)sources.size()) return -1;
  Clip c = makeClip(s, 0);
  c.in = clampd(in, 0, sources[s]); c.out = clampd(out, c.in + 0.05, sources[s]);
  S.clips.push_back(c);
  return c.id;
}

EMSCRIPTEN_KEEPALIVE int eng_clip_count() { return (int)S.clips.size(); }
EMSCRIPTEN_KEEPALIVE int eng_clip_id(int i) { return okI(i) ? S.clips[i].id : -1; }
EMSCRIPTEN_KEEPALIVE int eng_clip_src(int i) { return okI(i) ? S.clips[i].src : -1; }
EMSCRIPTEN_KEEPALIVE double eng_clip_in(int i) { return okI(i) ? S.clips[i].in : 0; }
EMSCRIPTEN_KEEPALIVE double eng_clip_out(int i) { return okI(i) ? S.clips[i].out : 0; }
EMSCRIPTEN_KEEPALIVE double eng_clip_len(int i) { return okI(i) ? len(S.clips[i]) : 0; }
EMSCRIPTEN_KEEPALIVE int eng_index_of(int id) { return findId(S.clips, id); }

EMSCRIPTEN_KEEPALIVE double eng_clip_start(int i) {
  double t = 0;
  for (int k = 0; k < i && k < (int)S.clips.size(); ++k) t += len(S.clips[k]);
  return t;
}
EMSCRIPTEN_KEEPALIVE double eng_duration() {
  double t = 0;
  for (auto& c : S.clips) t += len(c);
  return t;
}
// End of all content (video, text, audio).
EMSCRIPTEN_KEEPALIVE double eng_total() {
  double t = eng_duration();
  for (auto& o : S.ovs) t = std::max(t, o.start + o.dur);
  for (auto& a : S.aus) t = std::max(t, a.start + (a.out - a.in));
  return t;
}

// Maps a timeline time to (clip index, source time, local time).
EMSCRIPTEN_KEEPALIVE int eng_locate(double t) {
  double s = 0;
  for (size_t i = 0; i < S.clips.size(); ++i) {
    double l = len(S.clips[i]);
    if (t >= s && t < s + l) { qLocal = t - s; qSrc = S.clips[i].in + qLocal * spd(S.clips[i]); return (int)i; }
    s += l;
  }
  if (!S.clips.empty() && t >= s && t < s + 0.0005) {
    const Clip& c = S.clips.back();
    qLocal = std::max(0.0, len(c) - 0.001); qSrc = c.in + qLocal * spd(c);
    return (int)S.clips.size() - 1;
  }
  qLocal = 0; qSrc = 0;
  return -1;
}
EMSCRIPTEN_KEEPALIVE double eng_q_src() { return qSrc; }
EMSCRIPTEN_KEEPALIVE double eng_q_local() { return qLocal; }

// Fade multiplier (0..1) of clip i at local time (used for picture and sound).
EMSCRIPTEN_KEEPALIVE double eng_fade(int i, double local) {
  if (!okI(i)) return 0;
  const Clip& c = S.clips[i];
  double l = len(c), o = 1.0;
  if (c.p[P_FADEIN] > 0) o = std::min(o, local / c.p[P_FADEIN]);
  if (c.p[P_FADEOUT] > 0) o = std::min(o, (l - local) / c.p[P_FADEOUT]);
  return clampd(o, 0, 1);
}
// Picture opacity = fade * opacity param.
EMSCRIPTEN_KEEPALIVE double eng_opacity(int i, double local) {
  return okI(i) ? eng_fade(i, local) * clampd(S.clips[i].p[P_OPACITY], 0, 1) : 0;
}
// Effective transition length into clip i (0 when none).
EMSCRIPTEN_KEEPALIVE double eng_trans_len(int i) {
  if (!okI(i) || i == 0) return 0;
  const Clip& c = S.clips[i];
  if (c.p[P_TRTYPE] < 0.5 || c.p[P_TRDUR] <= 0) return 0;
  return std::min<double>(c.p[P_TRDUR], std::min(len(c), len(S.clips[i - 1])) * 0.9);
}
// Progress 0..1 of the transition into clip i, or -1 when outside it.
EMSCRIPTEN_KEEPALIVE double eng_trans_progress(int i, double local) {
  double d = eng_trans_len(i);
  if (d <= 0 || local >= d) return -1;
  return clampd(local / d, 0, 1);
}

static int splitAt(double t) {
  int i = eng_locate(t);
  if (i < 0) return -1;
  double local = qLocal;
  if (local < 0.05 || len(S.clips[i]) - local < 0.05) return -1;
  Clip b = S.clips[i];
  S.clips[i].out = qSrc;
  S.clips[i].p[P_FADEOUT] = 0;
  b.in = qSrc; b.id = nextId++; b.p[P_FADEIN] = 0; b.p[P_TRTYPE] = 0;
  S.clips.insert(S.clips.begin() + i + 1, b);
  return i + 1;
}
EMSCRIPTEN_KEEPALIVE int eng_split(double t) {
  int i = eng_locate(t);
  if (i < 0 || qLocal < 0.05 || len(S.clips[i]) - qLocal < 0.05) return -1;
  snap();
  int k = splitAt(t);
  return k >= 0 ? S.clips[k].id : -1;
}

// Insert a source at timeline time t (splitting the clip under it). length<=0 -> whole source.
EMSCRIPTEN_KEEPALIVE int eng_insert_clip(int s, double t, double length) {
  if (s < 0 || s >= (int)sources.size() || sources[s] <= 0) return -1;
  snap();
  Clip c = makeClip(s, length);
  int pos;
  int i = eng_locate(t);
  if (i < 0) pos = (int)S.clips.size();
  else if (qLocal < 0.05) pos = i;
  else if (len(S.clips[i]) - qLocal < 0.05) pos = i + 1;
  else pos = splitAt(t);
  S.clips.insert(S.clips.begin() + pos, c);
  return c.id;
}

// Ripple delete.
EMSCRIPTEN_KEEPALIVE int eng_delete(int id) {
  int i = findId(S.clips, id);
  if (i < 0) return 0;
  snap(); S.clips.erase(S.clips.begin() + i); return 1;
}
EMSCRIPTEN_KEEPALIVE int eng_duplicate(int id) {
  int i = findId(S.clips, id);
  if (i < 0) return -1;
  snap(); Clip c = S.clips[i]; c.id = nextId++;
  S.clips.insert(S.clips.begin() + i + 1, c); return c.id;
}
EMSCRIPTEN_KEEPALIVE int eng_move(int id, int to) {
  int i = findId(S.clips, id);
  if (i < 0) return 0;
  to = std::max(0, std::min(to, (int)S.clips.size() - 1));
  if (to == i) return 0;
  snap(); Clip c = S.clips[i];
  S.clips.erase(S.clips.begin() + i);
  S.clips.insert(S.clips.begin() + to, c);
  return 1;
}
// Live trim in source seconds (call eng_commit() once before a drag for undo).
EMSCRIPTEN_KEEPALIVE void eng_trim(int id, double in, double out) {
  int i = findId(S.clips, id);
  if (i < 0) return;
  double d = sources[S.clips[i].src];
  in = clampd(in, 0, d - 0.1);
  out = clampd(out, in + 0.1, d);
  S.clips[i].in = in; S.clips[i].out = out;
}

EMSCRIPTEN_KEEPALIVE void eng_set_param(int id, int p, float v) {
  int i = findId(S.clips, id);
  if (i < 0 || p < 0 || p >= P_COUNT) return;
  if (p == P_SPEED) v = std::max(0.1f, std::min(8.f, v));
  S.clips[i].p[p] = v;
}
EMSCRIPTEN_KEEPALIVE float eng_get_param(int id, int p) {
  int i = findId(S.clips, id);
  return (i >= 0 && p >= 0 && p < P_COUNT) ? S.clips[i].p[p] : 0.f;
}
EMSCRIPTEN_KEEPALIVE float eng_param_i(int i, int p) { return (okI(i) && p >= 0 && p < P_COUNT) ? S.clips[i].p[p] : 0.f; }
EMSCRIPTEN_KEEPALIVE float eng_param_default(int p) { return (p >= 0 && p < P_COUNT) ? DEF[p] : 0.f; }
EMSCRIPTEN_KEEPALIVE int eng_param_count() { return P_COUNT; }
// Reset params in [from, to) (pass 0, P_COUNT for all).
EMSCRIPTEN_KEEPALIVE void eng_reset_params(int id, int from, int to) {
  int i = findId(S.clips, id);
  if (i < 0) return;
  snap();
  for (int k = std::max(0, from); k < std::min(to, (int)P_COUNT); ++k) S.clips[i].p[k] = DEF[k];
}
// Copy params [from,to) of clip id to every clip.
EMSCRIPTEN_KEEPALIVE void eng_apply_to_all(int id, int from, int to) {
  int i = findId(S.clips, id);
  if (i < 0) return;
  snap();
  for (auto& c : S.clips) for (int k = std::max(0, from); k < std::min(to, (int)P_COUNT); ++k) c.p[k] = S.clips[i].p[k];
}

// ================= Text track =================
EMSCRIPTEN_KEEPALIVE int eng_ov_add(double start, double dur, int rec) {
  if (rec) snap();
  Ov o; o.id = nextId++; o.start = std::max(0.0, start); o.dur = std::max(0.1, dur);
  for (int i = 0; i < O_COUNT; ++i) o.q[i] = ODEF[i];
  o.text = "Your text";
  S.ovs.push_back(o);
  return o.id;
}
EMSCRIPTEN_KEEPALIVE int eng_ov_count() { return (int)S.ovs.size(); }
EMSCRIPTEN_KEEPALIVE int eng_ov_id(int i) { return okO(i) ? S.ovs[i].id : -1; }
EMSCRIPTEN_KEEPALIVE int eng_ov_index(int id) { return findId(S.ovs, id); }
EMSCRIPTEN_KEEPALIVE double eng_ov_start(int id) { int i = findId(S.ovs, id); return i < 0 ? 0 : S.ovs[i].start; }
EMSCRIPTEN_KEEPALIVE double eng_ov_dur(int id) { int i = findId(S.ovs, id); return i < 0 ? 0 : S.ovs[i].dur; }
EMSCRIPTEN_KEEPALIVE void eng_ov_set_time(int id, double start, double dur) {
  int i = findId(S.ovs, id);
  if (i < 0) return;
  S.ovs[i].start = std::max(0.0, start); S.ovs[i].dur = std::max(0.1, dur);
}
EMSCRIPTEN_KEEPALIVE void eng_ov_set(int id, int k, double v) {
  int i = findId(S.ovs, id);
  if (i >= 0 && k >= 0 && k < O_COUNT) S.ovs[i].q[k] = v;
}
EMSCRIPTEN_KEEPALIVE double eng_ov_get(int id, int k) {
  int i = findId(S.ovs, id);
  return (i >= 0 && k >= 0 && k < O_COUNT) ? S.ovs[i].q[k] : 0;
}
EMSCRIPTEN_KEEPALIVE void eng_ov_set_text(int id, const char* s) {
  int i = findId(S.ovs, id);
  if (i >= 0) S.ovs[i].text = s ? s : "";
}
EMSCRIPTEN_KEEPALIVE const char* eng_ov_text(int id) {
  int i = findId(S.ovs, id);
  strBuf = i >= 0 ? S.ovs[i].text : std::string();
  return strBuf.c_str();
}
EMSCRIPTEN_KEEPALIVE int eng_ov_delete(int id) {
  int i = findId(S.ovs, id);
  if (i < 0) return 0;
  snap(); S.ovs.erase(S.ovs.begin() + i); return 1;
}
EMSCRIPTEN_KEEPALIVE int eng_ov_duplicate(int id) {
  int i = findId(S.ovs, id);
  if (i < 0) return -1;
  snap(); Ov o = S.ovs[i]; o.id = nextId++; o.start += o.dur;
  S.ovs.push_back(o); return o.id;
}
EMSCRIPTEN_KEEPALIVE int eng_ov_split(int id, double t) {
  int i = findId(S.ovs, id);
  if (i < 0) return -1;
  Ov& o = S.ovs[i];
  if (t <= o.start + 0.05 || t >= o.start + o.dur - 0.05) return -1;
  snap();
  Ov b = S.ovs[i]; b.id = nextId++;
  b.start = t; b.dur = S.ovs[i].start + S.ovs[i].dur - t;
  S.ovs[i].dur = t - S.ovs[i].start;
  S.ovs.push_back(b);
  return b.id;
}

// ================= Audio track =================
EMSCRIPTEN_KEEPALIVE int eng_au_add(int s, double start, int lane, int rec) {
  if (s < 0 || s >= (int)sources.size() || sources[s] <= 0) return -1;
  if (rec) snap();
  Au a; a.id = nextId++; a.src = s; a.start = std::max(0.0, start); a.in = 0; a.out = sources[s];
  for (int i = 0; i < A_COUNT; ++i) a.a[i] = ADEF[i];
  a.a[A_LANE] = lane;
  S.aus.push_back(a);
  return a.id;
}
EMSCRIPTEN_KEEPALIVE int eng_au_count() { return (int)S.aus.size(); }
EMSCRIPTEN_KEEPALIVE int eng_au_id(int i) { return okA(i) ? S.aus[i].id : -1; }
EMSCRIPTEN_KEEPALIVE int eng_au_index(int id) { return findId(S.aus, id); }
EMSCRIPTEN_KEEPALIVE int eng_au_src(int id) { int i = findId(S.aus, id); return i < 0 ? -1 : S.aus[i].src; }
EMSCRIPTEN_KEEPALIVE double eng_au_start(int id) { int i = findId(S.aus, id); return i < 0 ? 0 : S.aus[i].start; }
EMSCRIPTEN_KEEPALIVE double eng_au_in(int id) { int i = findId(S.aus, id); return i < 0 ? 0 : S.aus[i].in; }
EMSCRIPTEN_KEEPALIVE double eng_au_out(int id) { int i = findId(S.aus, id); return i < 0 ? 0 : S.aus[i].out; }
EMSCRIPTEN_KEEPALIVE void eng_au_set_time(int id, double start, double in, double out) {
  int i = findId(S.aus, id);
  if (i < 0) return;
  double d = srcDur(S.aus[i].src);
  in = clampd(in, 0, d - 0.1); out = clampd(out, in + 0.1, d);
  S.aus[i].start = std::max(0.0, start); S.aus[i].in = in; S.aus[i].out = out;
}
EMSCRIPTEN_KEEPALIVE void eng_au_set(int id, int k, double v) {
  int i = findId(S.aus, id);
  if (i >= 0 && k >= 0 && k < A_COUNT) S.aus[i].a[k] = v;
}
EMSCRIPTEN_KEEPALIVE double eng_au_get(int id, int k) {
  int i = findId(S.aus, id);
  return (i >= 0 && k >= 0 && k < A_COUNT) ? S.aus[i].a[k] : 0;
}
// Gain of audio item index i at timeline t, or -1 when the item is not playing.
EMSCRIPTEN_KEEPALIVE double eng_au_gain(int i, double t) {
  if (!okA(i)) return -1;
  const Au& a = S.aus[i];
  double local = t - a.start, l = a.out - a.in;
  if (local < 0 || local >= l) return -1;
  double g = 1;
  if (a.a[A_FIN] > 0) g = std::min(g, local / a.a[A_FIN]);
  if (a.a[A_FOUT] > 0) g = std::min(g, (l - local) / a.a[A_FOUT]);
  return clampd(g, 0, 1) * a.a[A_VOL];
}
EMSCRIPTEN_KEEPALIVE double eng_au_src_time(int i, double t) { return okA(i) ? S.aus[i].in + (t - S.aus[i].start) : 0; }
EMSCRIPTEN_KEEPALIVE int eng_au_src_i(int i) { return okA(i) ? S.aus[i].src : -1; }
EMSCRIPTEN_KEEPALIVE int eng_au_delete(int id) {
  int i = findId(S.aus, id);
  if (i < 0) return 0;
  snap(); S.aus.erase(S.aus.begin() + i); return 1;
}
EMSCRIPTEN_KEEPALIVE int eng_au_duplicate(int id) {
  int i = findId(S.aus, id);
  if (i < 0) return -1;
  snap(); Au a = S.aus[i]; a.id = nextId++; a.start += a.out - a.in;
  S.aus.push_back(a); return a.id;
}
EMSCRIPTEN_KEEPALIVE int eng_au_split(int id, double t) {
  int i = findId(S.aus, id);
  if (i < 0) return -1;
  double local = t - S.aus[i].start, l = S.aus[i].out - S.aus[i].in;
  if (local < 0.05 || local > l - 0.05) return -1;
  snap();
  Au b = S.aus[i]; b.id = nextId++;
  b.start = t; b.in = S.aus[i].in + local; b.a[A_FIN] = 0;
  S.aus[i].out = b.in; S.aus[i].a[A_FOUT] = 0;
  S.aus.push_back(b);
  return b.id;
}

// ================= Markers =================
EMSCRIPTEN_KEEPALIVE void eng_mark_add(double t, int rec) {
  if (rec) snap();
  S.marks.push_back(std::max(0.0, t));
  std::sort(S.marks.begin(), S.marks.end());
}
// Removes a marker within tol of t, otherwise adds one. Returns 1 if added.
EMSCRIPTEN_KEEPALIVE int eng_mark_toggle(double t, double tol) {
  snap();
  for (size_t i = 0; i < S.marks.size(); ++i)
    if (std::fabs(S.marks[i] - t) <= tol) { S.marks.erase(S.marks.begin() + i); return 0; }
  S.marks.push_back(std::max(0.0, t));
  std::sort(S.marks.begin(), S.marks.end());
  return 1;
}
EMSCRIPTEN_KEEPALIVE void eng_mark_clear() { snap(); S.marks.clear(); }
EMSCRIPTEN_KEEPALIVE int eng_mark_count() { return (int)S.marks.size(); }
EMSCRIPTEN_KEEPALIVE double eng_mark_get(int i) { return (i >= 0 && i < (int)S.marks.size()) ? S.marks[i] : 0; }
// Next (dir>0) / previous (dir<0) edit point or marker from t; returns t if none.
EMSCRIPTEN_KEEPALIVE double eng_next_point(double t, int dir) {
  std::vector<double> pts(S.marks);
  double s = 0; pts.push_back(0);
  for (auto& c : S.clips) { s += len(c); pts.push_back(s); }
  for (auto& o : S.ovs) { pts.push_back(o.start); pts.push_back(o.start + o.dur); }
  for (auto& a : S.aus) { pts.push_back(a.start); pts.push_back(a.start + a.out - a.in); }
  double best = t, bd = 1e18;
  for (double p : pts) {
    double d = (p - t) * (dir > 0 ? 1 : -1);
    if (d > 0.002 && d < bd) { bd = d; best = p; }
  }
  return best;
}

// ================= Snapping =================
// Snap t to the nearest edit point within tol. exId's own edges are ignored.
EMSCRIPTEN_KEEPALIVE double eng_snap(double t, double tol, int exId, double playhead) {
  double best = t, bd = tol;
  auto test = [&](double p) { double d = std::fabs(p - t); if (d < bd) { bd = d; best = p; } };
  test(0); test(playhead);
  double s = 0;
  for (auto& c : S.clips) { test(s); s += len(c); test(s); }
  for (auto& o : S.ovs) if (o.id != exId) { test(o.start); test(o.start + o.dur); }
  for (auto& a : S.aus) if (a.id != exId) { test(a.start); test(a.start + a.out - a.in); }
  for (double m : S.marks) test(m);
  return best;
}

// ================= Undo / redo =================
EMSCRIPTEN_KEEPALIVE void eng_commit() { snap(); }
EMSCRIPTEN_KEEPALIVE int eng_can_undo() { return undoS.empty() ? 0 : 1; }
EMSCRIPTEN_KEEPALIVE int eng_can_redo() { return redoS.empty() ? 0 : 1; }
EMSCRIPTEN_KEEPALIVE int eng_undo() {
  if (undoS.empty()) return 0;
  redoS.push_back(S); S = undoS.back(); undoS.pop_back(); return 1;
}
EMSCRIPTEN_KEEPALIVE int eng_redo() {
  if (redoS.empty()) return 0;
  undoS.push_back(S); S = redoS.back(); redoS.pop_back(); return 1;
}

}
