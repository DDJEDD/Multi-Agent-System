#!/usr/bin/env python3
"""Plugin 'ffmpeg': assembles a short video from a JSON project.

Protocol: one JSON object on stdin {function, args, context},
one JSON object on stdout {"ok": bool, "message": str}.
Nothing else may be printed to stdout.

Function: renderVideo(project | filename)
  project: object (or JSON string), see "Project" below.

Settings (config.ini, section of this plugin), all optional:
  ffmpeg_path       path to ffmpeg                             (default: ffmpeg)
  ffprobe_path      path to ffprobe                            (default: ffprobe)
  timeout_s         timeout for one render, seconds            (default: 900)
  font_name         font family for subtitles                  (default: Arial)
  fonts_dir         extra folder with .ttf/.otf files          (default: none)
  default_size      WxH of the output                          (default: 720x1280)
  fps               output frame rate                          (default: 30)
  crf               x264 quality, lower = better               (default: 20)
  preset            x264 preset                                (default: veryfast)
  music_volume      default music volume 0..1                  (default: 0.2)
  loudnorm          true/false, normalize to -14 LUFS          (default: true)
  words_per_cue     subtitle chunk size in words               (default: 3)
  input_roots       folders (separated by ;) inputs may come from; empty = any
  queue_wait_s      how long a call may wait in the queue      (default: 3600)

Project (JSON):
{
  "output": "vickers.mp4",
  "size": "720x1280", "fps": 30,
  "transition": {"type": "fade", "duration": 0.3},        # default for all joints
  "clips": [ "a.mp4" | {"file": "a.mp4", "trim": 6, "start": 0,
                        "transition": {"type": "slideleft", "duration": 0.25}} ],
  "voice": [ "b1.wav" | {"file": "b1.wav", "block": 1, "offset": 0.1, "volume": 1} ],
  "voice_offset": 0.0,
  "music": {"file": "m.mp3", "volume": 0.2, "duck": true, "fade_out": 1.5},
  "clip_audio": 0,                       # volume of the clips' own sound, 0 = off
  "fit": "auto" | "video" | "voice" | "off"   # подгонка клипов и озвучки по длине
         | {"mode": "auto", "pad": 0.4, "max_audio_speed": 1.15,
            "min_audio_speed": 0.9, "min_video_speed": 0.85},
  "subtitles": [ {"block": 1, "text": "..."}                # timed by the voice
                 | {"start": 1.0, "end": 3.5, "text": "..."} ],
  "accents":   [ {"block": 1, "start": 0.2, "end": 2.5, "text": "12 ЧАСОВ"} ],
  "style": {"font": "Arial", "sub_size": 58, "accent_size": 84,
            "sub_color": "FFFFFF", "accent_color": "FFD400"}
}
Block numbers are 1-based and mean "clip number N". For items with "block",
start/end are relative to the beginning of that block.
"""
import json
import math
import os
import re
import shutil
import signal
import subprocess
import sys
import time
import uuid
from pathlib import Path

WIN = sys.platform == "win32"
CREATE_NO_WINDOW = 0x08000000
VIDEO_EXT = {".mp4", ".mov", ".webm", ".mkv"}
AUDIO_EXT = {".mp3", ".wav", ".m4a", ".aac", ".ogg", ".opus", ".flac"}
XFADE = {"fade", "fadeblack", "fadewhite", "fadegrays", "dissolve", "pixelize",
         "wipeleft", "wiperight", "wipeup", "wipedown",
         "slideleft", "slideright", "slideup", "slidedown",
         "smoothleft", "smoothright", "smoothup", "smoothdown",
         "circleopen", "circleclose", "radial", "zoomin",
         "hblur", "vertopen", "vertclose", "horzopen", "horzclose"}
ANSI_RE = re.compile(r"\x1b\[[0-9;?]*[A-Za-z]")
MAX_CLIPS = 12


def reply(ok, message):
    # ensure_ascii=True keeps stdout pure ASCII (Windows console code pages).
    sys.stdout.write(json.dumps({"ok": bool(ok), "message": str(message)}) + "\n")
    sys.stdout.flush()
    sys.exit(0)


# ---------------------------------------------------------------- helpers

def as_int(value, default, lo=None, hi=None):
    try:
        n = int(str(value).strip())
    except (TypeError, ValueError):
        n = default
    if lo is not None:
        n = max(lo, n)
    if hi is not None:
        n = min(hi, n)
    return n


def as_num(value, default, lo=None, hi=None):
    try:
        n = float(str(value).strip().replace(",", "."))
        if math.isnan(n) or math.isinf(n):
            raise ValueError
    except (TypeError, ValueError):
        n = default
    if lo is not None:
        n = max(lo, n)
    if hi is not None:
        n = min(hi, n)
    return n


def as_bool(value, default=False):
    if value is None:
        return default
    return str(value).strip().lower() in ("1", "true", "yes", "on")


def clean_tail(text, limit=600):
    text = ANSI_RE.sub("", text or "").strip()
    return re.sub(r"[ \t]+", " ", text)[-limit:]


def safe_filename(raw):
    name = re.sub(r"[^\w\-.]", "_", str(raw).strip()).lstrip(".")
    if not name:
        return None
    if not name.lower().endswith(".mp4"):
        name += ".mp4"
    stem, ext = os.path.splitext(name)
    return stem[:80] + ext


def unique_path(out_dir, name):
    p = out_dir / name
    if not p.exists():
        return p
    stem, ext = os.path.splitext(name)
    for i in range(2, 1000):
        q = out_dir / ("%s_%d%s" % (stem, i, ext))
        if not q.exists():
            return q
    return out_dir / ("%s_%d%s" % (stem, int(time.time()), ext))


def kill_tree(proc):
    try:
        if WIN:
            subprocess.run(["taskkill", "/PID", str(proc.pid), "/T", "/F"],
                           capture_output=True, timeout=15,
                           creationflags=CREATE_NO_WINDOW)
        else:
            os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
    except Exception:
        try:
            proc.kill()
        except Exception:
            pass


def run_cmd(cmd, timeout, cwd=None):
    """(returncode | None on timeout, stdout, stderr, timed_out). No shell."""
    kw = {}
    if WIN:
        kw["creationflags"] = CREATE_NO_WINDOW
    else:
        kw["start_new_session"] = True
    proc = subprocess.Popen(cmd, stdin=subprocess.DEVNULL,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                            text=True, encoding="utf-8", errors="replace",
                            cwd=cwd, **kw)
    try:
        out, err = proc.communicate(timeout=timeout)
        return proc.returncode, out or "", err or "", False
    except subprocess.TimeoutExpired:
        kill_tree(proc)
        try:
            out, err = proc.communicate(timeout=10)
        except Exception:
            out, err = "", ""
        return None, out or "", err or "", True


class Lock:
    """One render at a time (CPU heavy, and keeps the queue predictable)."""

    def __init__(self, path, stale_s):
        self.path, self.stale_s, self.held = path, stale_s, False

    def acquire(self):
        for _ in range(2):
            try:
                fd = os.open(str(self.path), os.O_CREAT | os.O_EXCL | os.O_WRONLY)
                os.write(fd, str(os.getpid()).encode())
                os.close(fd)
                self.held = True
                return True
            except FileExistsError:
                try:
                    if time.time() - os.path.getmtime(self.path) > self.stale_s:
                        os.remove(self.path)
                        continue
                except OSError:
                    pass
                return False
            except OSError:
                return True
        return False

    def release(self):
        if self.held:
            try:
                os.remove(str(self.path))
            except OSError:
                pass
            self.held = False


class Gate:
    """FIFO queue in front of the Lock (ticket files, stale after 60 s)."""

    def __init__(self, data_dir, lock):
        self.qdir = data_dir / "queue"
        self.lock = lock
        self.ticket = None

    def _live(self):
        now, names = time.time(), []
        try:
            for t in self.qdir.glob("*.t"):
                try:
                    if now - t.stat().st_mtime > 60:
                        t.unlink()
                        continue
                except OSError:
                    continue
                names.append(t.name)
        except OSError:
            pass
        return sorted(names)

    def wait(self, max_wait_s):
        try:
            self.qdir.mkdir(parents=True, exist_ok=True)
            self.ticket = self.qdir / ("%020d_%d.t" % (time.time_ns(), os.getpid()))
            self.ticket.write_text("", encoding="utf-8")
        except OSError:
            self.ticket = None
        deadline = time.time() + max_wait_s
        try:
            while True:
                first = True
                if self.ticket is not None:
                    live = self._live()
                    first = (not live) or live[0] == self.ticket.name
                if first and self.lock.acquire():
                    return True
                if time.time() >= deadline:
                    return False
                if self.ticket is not None:
                    try:
                        os.utime(str(self.ticket), None)
                    except OSError:
                        pass
                time.sleep(2)
        finally:
            if self.ticket is not None:
                try:
                    self.ticket.unlink()
                except OSError:
                    pass


# ------------------------------------------------------- project parsing

class Ctx:
    pass


def check_file(raw, exts, what, ctx):
    p = Path(str(raw)).expanduser()
    try:
        p = p.resolve()
    except OSError:
        reply(False, "%s: недопустимый путь" % what)
    if p.suffix.lower() not in exts:
        reply(False, "%s: неподдерживаемый тип файла %s" % (what, p.suffix))
    if not p.is_file():
        reply(False, "%s: файл не найден: %s" % (what, p))
    if ctx.roots and not any(r == p or r in p.parents for r in ctx.roots):
        reply(False, "%s: файл вне разрешённых папок (input_roots)" % what)
    return p


def find_ffprobe(ctx):
    """ffprobe from settings/PATH, or the one next to ffmpeg. None if absent."""
    found = shutil.which(ctx.ffprobe)
    if found:
        return found
    ff = shutil.which(ctx.ffmpeg) or ctx.ffmpeg
    sib = Path(ff).with_name("ffprobe.exe" if WIN else "ffprobe")
    return str(sib) if sib.is_file() else None


def probe(path, ctx):
    """(duration, has_audio, has_video). Uses ffprobe; without it falls back
    to parsing the banner of `ffmpeg -i`."""
    if not hasattr(ctx, "probe_bin"):
        ctx.probe_bin = find_ffprobe(ctx)
    try:
        if ctx.probe_bin:
            cmd = [ctx.probe_bin, "-v", "error", "-print_format", "json",
                   "-show_entries", "format=duration:stream=codec_type", str(path)]
            code, out, err, timed_out = run_cmd(cmd, 60)
            try:
                data = json.loads(out)
                dur = float(data["format"]["duration"])
                kinds = [s.get("codec_type") for s in data.get("streams", [])]
                return dur, "audio" in kinds, "video" in kinds
            except Exception:
                reply(False, "не удалось прочитать файл %s: %s"
                      % (path.name, clean_tail(err or out, 200)))
        code, out, err, timed_out = run_cmd(
            [ctx.ffmpeg, "-hide_banner", "-i", str(path)], 60)
    except OSError:
        reply(False, "ffmpeg не найден, проверьте ffmpeg_path в config.ini")
    text = ANSI_RE.sub("", (err or "") + (out or ""))
    m = re.search(r"Duration:\s*(\d+):(\d+):(\d+(?:\.\d+)?)", text)
    if not m:
        reply(False, "не удалось прочитать файл %s: %s"
              % (path.name, clean_tail(text, 200)))
    dur = int(m.group(1)) * 3600 + int(m.group(2)) * 60 + float(m.group(3))
    return (dur, bool(re.search(r"Stream #\S+.*: Audio:", text)),
            bool(re.search(r"Stream #\S+.*: Video:", text)))


def parse_transition(raw, default, fps):
    t = dict(default)
    if isinstance(raw, dict):
        t["type"] = str(raw.get("type", t["type"])).strip().lower()
        t["duration"] = as_num(raw.get("duration"), t["duration"], 0, 3)
    elif isinstance(raw, str):
        t["type"] = raw.strip().lower()
    if t["type"] in ("none", "cut", ""):
        t = {"type": "fade", "duration": 0.0}
    if t["type"] not in XFADE:
        reply(False, "неизвестный переход %s. Допустимые: %s"
              % (t["type"], ", ".join(sorted(XFADE))))
    return t


def ass_time(sec):
    sec = max(0.0, sec)
    cs = int(round(sec * 100))
    return "%d:%02d:%02d.%02d" % (cs // 360000, cs // 6000 % 60, cs // 100 % 60, cs % 100)


def ass_text(text):
    text = str(text).replace("\r", "").strip()
    text = text.replace("{", "(").replace("}", ")")
    return re.sub(r"\s*\n\s*", r"\\N", text)


def split_words(text, per):
    words = str(text).split()
    if not words:
        return []
    n = max(1, int(math.ceil(len(words) / float(per))))
    size = int(math.ceil(len(words) / float(n)))
    return [" ".join(words[i:i + size]) for i in range(0, len(words), size)]


def ass_color(hex6, default):
    h = str(hex6 or default).strip().lstrip("#")
    if not re.fullmatch(r"[0-9a-fA-F]{6}", h):
        h = default
    return "&H00%s%s%s" % (h[4:6], h[2:4], h[0:2])  # RGB -> BGR


def build_ass(ctx, cues, accents, style):
    W, H = ctx.w, ctx.h
    font = str(style.get("font") or ctx.font)
    font = re.sub(r"[,\r\n]", " ", font)
    sub_size = as_int(style.get("sub_size"), int(H * 0.04), 12, 300)
    acc_size = as_int(style.get("accent_size"), int(H * 0.065), 12, 400)
    lines = [
        "[Script Info]", "ScriptType: v4.00+", "PlayResX: %d" % W,
        "PlayResY: %d" % H, "WrapStyle: 0", "ScaledBorderAndShadow: yes", "",
        "[V4+ Styles]",
        "Format: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, "
        "OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, ScaleX, "
        "ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, Alignment, "
        "MarginL, MarginR, MarginV, Encoding",
        "Style: Sub,%s,%d,%s,&H000000FF,&H00000000,&H64000000,-1,0,0,0,100,100,0,0,1,4,1,2,50,50,%d,1"
        % (font, sub_size, ass_color(style.get("sub_color"), "FFFFFF"), int(H * 0.2)),
        "Style: Accent,%s,%d,%s,&H000000FF,&H00000000,&H64000000,-1,0,0,0,100,100,0,0,1,5,2,8,50,50,%d,1"
        % (font, acc_size, ass_color(style.get("accent_color"), "FFD400"), int(H * 0.15)),
        "", "[Events]",
        "Format: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text",
    ]
    for start, end, text in cues:
        lines.append("Dialogue: 0,%s,%s,Sub,,0,0,0,,{\\fad(40,40)\\fscx85\\fscy85"
                     "\\t(0,110,\\fscx100\\fscy100)}%s"
                     % (ass_time(start), ass_time(end), ass_text(text)))
    for start, end, text in accents:
        lines.append("Dialogue: 1,%s,%s,Accent,,0,0,0,,{\\fad(120,120)}%s"
                     % (ass_time(start), ass_time(end), ass_text(text)))
    return "\n".join(lines) + "\n"


def fesc(path):
    """Escape a path for use inside a quoted ffmpeg filter option."""
    return str(path).replace("\\", "/").replace(":", "\\:").replace("'", "\\'")


# ------------------------------------------------------------------ main

def load_local_ini():
    """Fallback when the host passes no settings: config.ini next to the
    script, section [ffmpeg]."""
    import configparser
    ini = Path(__file__).resolve().parent / "config.ini"
    cp = configparser.ConfigParser()
    try:
        cp.read(str(ini), encoding="utf-8")
        return dict(cp["ffmpeg"]) if cp.has_section("ffmpeg") else {}
    except Exception:
        return {}


def handle(req):
    if req.get("function") != "renderVideo":
        reply(False, "неизвестная функция " + str(req.get("function")))
    args = req.get("args") if isinstance(req.get("args"), dict) else {}
    cx = req.get("context") if isinstance(req.get("context"), dict) else {}
    cfg = cx.get("settings") if isinstance(cx.get("settings"), dict) else {}
    if not cfg:
        cfg = load_local_ini()

    ctx = Ctx()
    ctx.ffmpeg = str(cfg.get("ffmpeg_path") or "ffmpeg").strip()
    ctx.ffprobe = str(cfg.get("ffprobe_path") or "ffprobe").strip()
    ctx.font = str(cfg.get("font_name") or "Arial").strip()
    ctx.fonts_dir = str(cfg.get("fonts_dir") or "").strip()
    ctx.roots = []
    for part in str(cfg.get("input_roots") or "").split(";"):
        if part.strip():
            try:
                ctx.roots.append(Path(part.strip()).expanduser().resolve())
            except OSError:
                pass
    timeout = as_int(cfg.get("timeout_s"), 900, 60, 7200)

    data_dir = cx.get("dataDir")
    if not data_dir:
        reply(False, "в context нет dataDir")
    data_dir = Path(str(data_dir))
    out_dir = data_dir / "out"
    try:
        out_dir.mkdir(parents=True, exist_ok=True)
    except OSError as e:
        reply(False, "не удалось создать папку вывода: %s" % e)

    # ---- project
    proj = args.get("project", args)
    if isinstance(proj, str):
        try:
            proj = json.loads(proj)
        except ValueError:
            reply(False, "project: невалидный JSON")
    if not isinstance(proj, dict):
        reply(False, "project должен быть объектом")

    size = str(proj.get("size") or cfg.get("default_size") or "720x1280")
    m = re.fullmatch(r"(\d{3,4})x(\d{3,4})", size.strip().lower())
    if not m:
        reply(False, "недопустимый size, ожидается вид 720x1280")
    ctx.w, ctx.h = int(m.group(1)) // 2 * 2, int(m.group(2)) // 2 * 2
    fps = as_int(proj.get("fps") or cfg.get("fps"), 30, 15, 60)
    ctx.fps = fps

    raw_clips = proj.get("clips")
    if not isinstance(raw_clips, list) or not raw_clips:
        reply(False, "project.clips: нужен непустой список клипов")
    if len(raw_clips) > MAX_CLIPS:
        reply(False, "слишком много клипов (максимум %d)" % MAX_CLIPS)

    default_tr = parse_transition(proj.get("transition"),
                                  {"type": "fade", "duration": 0.3}, fps)

    clips = []
    for i, c in enumerate(raw_clips):
        item = {"file": c} if isinstance(c, str) else c
        if not isinstance(item, dict) or not item.get("file"):
            reply(False, "clips[%d]: нужен файл" % i)
        path = check_file(item["file"], VIDEO_EXT, "clips[%d]" % i, ctx)
        dur, has_a, has_v = probe(path, ctx)
        if not has_v:
            reply(False, "clips[%d]: в файле нет видео" % i)
        start = as_num(item.get("start"), 0.0, 0, 3600)
        length = as_num(item.get("trim"), dur - start, 0.5, 3600)
        length = min(length, dur - start)
        if length < 0.5:
            reply(False, "clips[%d]: слишком короткий отрезок" % i)
        tr = parse_transition(item.get("transition"), default_tr, fps) if i else None
        clips.append({"path": path, "start": start, "len": length,
                      "src": length, "speed": 1.0, "freeze": 0.0,
                      "avail": dur - start, "audio": has_a, "tr": tr})


    # ---- fit: подгонка длины клипов и озвучки друг под друга
    fit_raw = proj.get("fit")
    if isinstance(fit_raw, str):
        fit_raw = {"mode": fit_raw}
    if fit_raw is True:
        fit_raw = {"mode": "auto"}
    fit_raw = fit_raw if isinstance(fit_raw, dict) else {}
    fit_mode = str(fit_raw.get("mode") or ("auto" if fit_raw else "off")).strip().lower()
    if fit_mode not in ("off", "auto", "video", "voice"):
        reply(False, "fit.mode: допустимо auto, video, voice или off")
    voice_tempo = {}
    fit_notes = []   # проблемы -> «Внимание»
    fit_info = []    # что сделано -> «Подгонка»
    if fit_mode != "off":
        f_pad = as_num(fit_raw.get("pad"), 0.4, 0, 3)
        f_amax = as_num(fit_raw.get("max_audio_speed"), 1.15, 1.0, 1.5)
        f_amin = as_num(fit_raw.get("min_audio_speed"), 0.9, 0.7, 1.0)
        f_vmin = as_num(fit_raw.get("min_video_speed"), 0.85, 0.5, 1.0)
        fv = proj.get("voice") or []
        if isinstance(fv, str):
            fv = [fv]
        done_blocks = set()
        for j, v in enumerate(fv):
            item = {"file": v} if isinstance(v, str) else v
            if not isinstance(item, dict) or not item.get("file"):
                continue
            b = as_int(item.get("block"), 0, 0, 999)
            if not b and "start" not in item and isinstance(v, str):
                b = j + 1
            if not b or b > len(clips) or b in done_blocks:
                continue
            vpath = check_file(item["file"], AUDIO_EXT | VIDEO_EXT, "voice[%d]" % j, ctx)
            a_dur, a_has, _ = probe(vpath, ctx)
            if not a_has or a_dur <= 0.05:
                continue
            done_blocks.add(b)
            c = clips[b - 1]
            v0 = c["src"]
            tempo = 1.0
            if fit_mode == "voice":
                tempo = min(f_amax, max(f_amin, a_dur / max(0.3, v0 - f_pad)))
                if a_dur / tempo > v0 - f_pad + 0.05:
                    fit_notes.append("блок %d: озвучка длиннее клипа на %.1f с даже после ускорения"
                                     % (b, a_dur / tempo - (v0 - f_pad)))
                note = "блок %d: озвучка x%.2f" % (b, tempo)
            else:
                need = a_dur + f_pad
                if fit_mode == "auto" and need > v0:
                    tempo = min(f_amax, (need / v0) ** 0.5)
                    need = a_dur / tempo + f_pad
                s = max(0.5, min(c["avail"], need) if need > v0 else need)
                speed = 1.0
                if s < need - 0.02:
                    speed = max(f_vmin, s / need)
                out = s / speed
                freeze = max(0.0, need - out)
                c.update(src=s, speed=speed, freeze=freeze, len=out + freeze)
                note = "блок %d: клип %.1f->%.1f с" % (b, v0, out + freeze)
                if abs(speed - 1.0) > 0.005:
                    note += ", видео x%.2f" % speed
                if freeze > 0.05:
                    note += ", заморозка кадра %.1f с" % freeze
                if abs(tempo - 1.0) > 0.005:
                    note += ", озвучка x%.2f" % tempo
            if abs(tempo - 1.0) > 0.005:
                voice_tempo[j] = tempo
            fit_info.append(note)

    # ---- timeline: xfade joints shorten the video, so compute block starts
    min_d = 2.0 / fps
    acc = clips[0]["len"]
    clips[0].update(offset=0.0, block_start=0.0, d=0.0)
    for k in range(1, len(clips)):
        tr = clips[k]["tr"]
        d = tr["duration"] if tr["duration"] > 0 else min_d
        d = max(min_d, min(d, 0.5 * clips[k - 1]["len"], 0.5 * clips[k]["len"]))
        off = acc - d
        clips[k].update(offset=off, block_start=off + d / 2.0, d=d)
        acc = off + clips[k]["len"]
    total = acc
    nblocks = len(clips)

    def block_of(item, what):
        b = as_int(item.get("block"), 0, 0, 999)
        if b and b > nblocks:
            reply(False, "%s: блока %d нет (клипов %d)" % (what, b, nblocks))
        return b

    warnings = list(fit_notes)

    # ---- voice
    voices = []
    raw_voice = proj.get("voice") or []
    if isinstance(raw_voice, str):
        raw_voice = [raw_voice]
    voice_offset = as_num(proj.get("voice_offset"), 0.0, -2, 5)
    for j, v in enumerate(raw_voice):
        item = {"file": v} if isinstance(v, str) else v
        if not isinstance(item, dict) or not item.get("file"):
            reply(False, "voice[%d]: нужен файл" % j)
        path = check_file(item["file"], AUDIO_EXT | VIDEO_EXT, "voice[%d]" % j, ctx)
        dur, has_a, _ = probe(path, ctx)
        if not has_a:
            reply(False, "voice[%d]: в файле нет звука" % j)
        tempo = voice_tempo.get(j, 1.0)
        dur = dur / tempo
        b = block_of(item, "voice[%d]" % j)
        if not b and "start" not in item and isinstance(v, str):
            b = j + 1
            if b > nblocks:
                reply(False, "voice[%d]: больше озвучек, чем клипов" % j)
        base = clips[b - 1]["block_start"] if b else 0.0
        start = base + voice_offset * (1 if b else 0) \
            + as_num(item.get("offset") if b else item.get("start"), 0.0, -10, 3600)
        start = max(0.0, start)
        end = start + dur
        if end > total + 0.05:
            warnings.append("озвучка %d длиннее ролика на %.1f с, конец обрезан"
                            % (j + 1, end - total))
        elif b and b < nblocks and end > clips[b]["block_start"] + 0.15:
            warnings.append("озвучка блока %d заходит на следующий блок на %.1f с"
                            % (b, end - clips[b]["block_start"]))
        voices.append({"path": path, "start": start, "dur": dur, "tempo": tempo,
                       "block": b, "vol": as_num(item.get("volume"), 1.0, 0, 4)})

    # ---- subtitles and accents (ASS)
    wpc = as_int(cfg.get("words_per_cue"), 3, 1, 12)

    def timed(item, what, default_to_voice):
        b = block_of(item, what)
        text = str(item.get("text", "")).strip()
        if not text:
            return None
        if b:
            bs = clips[b - 1]["block_start"]
            be = (clips[b]["block_start"] if b < nblocks else total)
            vb = next((v for v in voices if v["block"] == b), None)
            if "start" in item or "end" in item:
                s = bs + as_num(item.get("start"), 0.0, 0, 600)
                e = bs + as_num(item.get("end"), be - bs, 0, 600)
            elif default_to_voice and vb:
                s, e = vb["start"], vb["start"] + vb["dur"]
            else:
                s, e = bs, be
        else:
            s = as_num(item.get("start"), 0.0, 0, 3600)
            e = as_num(item.get("end"), total, 0, 3600)
        e = min(e, total)
        if e - s < 0.1:
            return None
        return s, e, text

    cues, accents = [], []
    for i, it in enumerate(proj.get("subtitles") or []):
        if not isinstance(it, dict):
            reply(False, "subtitles[%d]: ожидается объект" % i)
        t = timed(it, "subtitles[%d]" % i, True)
        if not t:
            continue
        s, e, text = t
        chunks = split_words(text, wpc)
        weights = [max(1, len(c)) for c in chunks]
        cur = s
        for c, w in zip(chunks, weights):
            nxt = cur + (e - s) * w / float(sum(weights))
            cues.append((cur, nxt, c))
            cur = nxt
    for i, it in enumerate(proj.get("accents") or []):
        if not isinstance(it, dict):
            reply(False, "accents[%d]: ожидается объект" % i)
        t = timed(it, "accents[%d]" % i, False)
        if t:
            accents.append(t)

    # ---- music
    music = None
    mraw = proj.get("music")
    if isinstance(mraw, str):
        mraw = {"file": mraw}
    if isinstance(mraw, dict) and mraw.get("file"):
        path = check_file(mraw["file"], AUDIO_EXT | VIDEO_EXT, "music", ctx)
        _, has_a, _ = probe(path, ctx)
        if not has_a:
            reply(False, "music: в файле нет звука")
        music = {"path": path,
                 "vol": as_num(mraw.get("volume"), as_num(cfg.get("music_volume"), 0.2, 0, 1), 0, 2),
                 "duck": as_bool(mraw.get("duck"), True),
                 "fade": as_num(mraw.get("fade_out"), 1.5, 0, 10)}

    clip_audio = as_num(proj.get("clip_audio"), 0.0, 0, 2)

    # ---- output name
    raw_name = str(proj.get("output") or args.get("filename") or "").strip()
    name = safe_filename(raw_name) if raw_name else None
    name = name or ("short_%d.mp4" % int(time.time()))

    # ---- queue, then render
    lock = Lock(data_dir / "ffmpeg.lock", timeout + 120)
    gate = Gate(data_dir, lock)
    if not gate.wait(as_int(cfg.get("queue_wait_s"), 3600, 0, 86400)):
        reply(False, "очередь рендера не освободилась за отведённое время")
    work = data_dir / "work" / uuid.uuid4().hex[:12]
    try:
        work.mkdir(parents=True, exist_ok=True)
        final = unique_path(out_dir, name)
        tmp = work / "render.mp4"
        render(ctx, cfg, clips, voices, music, clip_audio, cues, accents,
               proj.get("style") if isinstance(proj.get("style"), dict) else {},
               total, work, tmp, timeout)
        shutil.move(str(tmp), str(final))
    finally:
        shutil.rmtree(str(work), ignore_errors=True)
        lock.release()

    msg = "Готово, файл: %s (%.1f с)" % (final, total)
    if fit_info:
        msg += ". Подгонка: " + "; ".join(fit_info)
    if warnings:
        msg += ". Внимание: " + "; ".join(warnings)
    reply(True, msg)


def render(ctx, cfg, clips, voices, music, clip_audio, cues, accents, style,
           total, work, tmp, timeout):
    n = len(clips)
    W, H, fps = ctx.w, ctx.h, ctx.fps
    cmd = [ctx.ffmpeg, "-y", "-hide_banner", "-loglevel", "error"]
    for c in clips:
        cmd += ["-i", str(c["path"])]
    for v in voices:
        cmd += ["-i", str(v["path"])]
    if music:
        cmd += ["-stream_loop", "-1", "-i", str(music["path"])]

    f = []
    # video: normalize every clip, then chain xfade
    for i, c in enumerate(clips):
        tp = (",tpad=stop_mode=clone:stop_duration=%.3f" % c["freeze"]
              if c["freeze"] > 0.01 else "")
        f.append("[%d:v]trim=start=%.3f:duration=%.3f,setpts=(PTS-STARTPTS)/%.4f%s,"
                 "scale=%d:%d:force_original_aspect_ratio=increase,crop=%d:%d,"
                 "setsar=1,fps=%d,format=yuv420p[v%d]"
                 % (i, c["start"], c["src"], c["speed"], tp, W, H, W, H, fps, i))
    last = "v0"
    for k in range(1, n):
        f.append("[%s][v%d]xfade=transition=%s:duration=%.3f:offset=%.3f[x%d]"
                 % (last, k, clips[k]["tr"]["type"], clips[k]["d"],
                    clips[k]["offset"], k))
        last = "x%d" % k

    if cues or accents:
        (work / "subs.ass").write_text(build_ass(ctx, cues, accents, style),
                                       encoding="utf-8")
        opt = "ass=subs.ass"
        if ctx.fonts_dir:
            opt += ":fontsdir='%s'" % fesc(ctx.fonts_dir)
        f.append("[%s]%s[vout]" % (last, opt))
    else:
        f.append("[%s]null[vout]" % last)

    # audio
    base = n
    mix = []
    stereo = "aresample=48000,aformat=channel_layouts=stereo"
    if voices:
        for j, v in enumerate(voices):
            ms = int(round(v["start"] * 1000))
            at = (",atempo=%.4f" % v["tempo"] if abs(v["tempo"] - 1.0) > 0.005 else "")
            f.append("[%d:a]%s%s,volume=%.3f,adelay=%d|%d[vo%d]"
                     % (base + j, stereo, at, v["vol"], ms, ms, j))
        if len(voices) > 1:
            f.append("%samix=inputs=%d:normalize=0:duration=longest[voice]"
                     % ("".join("[vo%d]" % j for j in range(len(voices))), len(voices)))
        else:
            f.append("[vo0]anull[voice]")
    if clip_audio > 0:
        cas = []
        for i, c in enumerate(clips):
            if not c["audio"]:
                continue
            ms = int(round(c["offset"] * 1000))
            ca_t = (",atempo=%.4f" % c["speed"] if abs(c["speed"] - 1.0) > 0.005 else "")
            f.append("[%d:a]atrim=start=%.3f:duration=%.3f,asetpts=PTS-STARTPTS%s,%s,"
                     "volume=%.3f,adelay=%d|%d[ca%d]"
                     % (i, c["start"], c["src"], ca_t, stereo, clip_audio, ms, ms, i))
            cas.append("[ca%d]" % i)
        if len(cas) > 1:
            f.append("%samix=inputs=%d:normalize=0:duration=longest[clipbus]"
                     % ("".join(cas), len(cas)))
            mix.append("[clipbus]")
        elif cas:
            mix.append(cas[0])
    if music:
        mi = base + len(voices)
        fade = min(music["fade"], total / 2.0)
        chain = ("[%d:a]%s,atrim=duration=%.3f,asetpts=PTS-STARTPTS,volume=%.3f,"
                 "afade=t=in:d=0.5" % (mi, stereo, total, music["vol"]))
        if fade > 0:
            chain += ",afade=t=out:st=%.3f:d=%.3f" % (total - fade, fade)
        f.append(chain + "[mus]")
        if voices and music["duck"]:
            f.append("[voice]asplit=2[vsc][vmix]")
            f.append("[mus][vsc]sidechaincompress=threshold=0.04:ratio=8:"
                     "attack=20:release=350[mduck]")
            mix += ["[vmix]", "[mduck]"]
        else:
            if voices:
                mix.append("[voice]")
            mix.append("[mus]")
    elif voices:
        mix.append("[voice]")

    has_audio = bool(mix)
    if has_audio:
        tail = ""
        if as_bool(cfg.get("loudnorm"), True):
            tail = "loudnorm=I=-14:TP=-1.5:LRA=11,"
        tail += "aresample=48000,atrim=duration=%.3f,asetpts=PTS-STARTPTS" % total
        if len(mix) > 1:
            f.append("%samix=inputs=%d:normalize=0:duration=longest,%s[aout]"
                     % ("".join(mix), len(mix), tail))
        else:
            f.append("%s%s[aout]" % (mix[0], tail))
        # a single-input mix label above is a plain chain, not amix

    cmd += ["-filter_complex", ";".join(f), "-map", "[vout]"]
    if has_audio:
        cmd += ["-map", "[aout]", "-c:a", "aac", "-b:a", "192k", "-ar", "48000"]
    else:
        cmd += ["-an"]
    cmd += ["-c:v", "libx264", "-preset", str(cfg.get("preset") or "veryfast"),
            "-crf", str(as_int(cfg.get("crf"), 20, 10, 35)), "-pix_fmt", "yuv420p",
            "-r", str(fps), "-t", "%.3f" % total, "-movflags", "+faststart",
            str(tmp)]

    try:
        code, out, err, timed_out = run_cmd(cmd, timeout, cwd=str(work))
    except FileNotFoundError:
        reply(False, "ffmpeg не найден, проверьте ffmpeg_path в config.ini")
    except OSError as e:
        reply(False, "не удалось запустить ffmpeg: %s" % e)
    if timed_out:
        reply(False, "ffmpeg не уложился во время (%d с)" % timeout)
    if code != 0 or not tmp.is_file() or tmp.stat().st_size == 0:
        reply(False, "ffmpeg завершился с кодом %s: %s"
              % (code, clean_tail(err or out)))


def main():
    try:
        req = json.loads(sys.stdin.buffer.read().decode("utf-8"))
        if not isinstance(req, dict):
            raise ValueError("not an object")
    except Exception:
        reply(False, "не удалось разобрать входной JSON")
    try:
        handle(req)
    except Exception as e:  # SystemExit from reply() is not caught here
        reply(False, "внутренняя ошибка плагина: %s: %s" % (type(e).__name__, e))


main()