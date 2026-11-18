#!/usr/bin/env python3
"""Plugin 'gflow': drives gflow-cli (unofficial Google Flow client).

Protocol: one JSON object on stdin {function, args, context},
one JSON object on stdout {"ok": bool, "message": str}.
Nothing else may be printed to stdout (stray print() goes to stderr).

Settings (config.ini, section of this plugin), all optional except allow_video:
  gflow_path            path to gflow / gflow.exe              (default: gflow)
  timeout_s             timeout for images, seconds            (default: 280)
  video_timeout_s       timeout for video, seconds             (default: 600)
  allow_video           true/false                             (default: false)
  default_aspect        e.g. 16:9                              (default: 16:9)
  max_duration_s        upper limit for the duration argument  (default: 8)
  max_video_count       upper limit for videos per call        (default: 1)
  video_min_interval_s  pause between two video generations    (default: 90, 0 = off)
  image_retries         extra attempts for a failed image      (default: 1)
  queue_wait_s          how long a call may wait in the queue  (default: 3600)
  default_resolution    e.g. 720p, used when not passed        (default: none)

Arguments: prompt (required), aspect (alias aspectRatio / aspect_ratio),
resolution (e.g. 720p), model, duration (video), count,
filename (video only, e.g. "1.mp4").

Concurrent calls are queued (first come, first served): gflow uses a single
browser profile, so jobs run strictly one after another.

Error format. The reader of "message" is an LLM agent, so every failure is
written the same way and tells the agent what to do next:

    ОШИБКА: <what happened>.
    Код: <stable_snake_case_code>
    Кредиты Flow: <not spent / maybe spent / spent>.
    Повтор: <may it retry: now / once / later / never / only after the user acts>.
    Что делать: <concrete next step for the agent or the user>
    Детали: <tail of gflow output, if any>
    Примечание: <things the plugin adjusted on its own>

Codes: bad_request, unknown_function, video_disabled, bad_argument,
prompt_too_long, gflow_not_found, gflow_launch_failed, out_dir_error,
disk_full, queue_timeout, auth_required, flow_ui_changed,
quota_or_rate_limit, content_rejected, profile_busy, network_error,
timeout, partial_result, download_failed, no_output_file, gflow_failed,
cancelled, plugin_bug.
"""
import json
import os
import re
import shutil
import signal
import subprocess
import sys
import time
import traceback
from pathlib import Path

_STDOUT = sys.stdout
sys.stdout = sys.stderr  # a stray print() must never corrupt the JSON reply

WIN = sys.platform == "win32"
CREATE_NO_WINDOW = 0x08000000
MEDIA_EXT = (".mp4", ".webm", ".mov", ".png", ".jpg", ".jpeg", ".webp")
VIDEO_EXT = (".mp4", ".webm", ".mov")
MAX_PROMPT = 2000
MAX_REPLY = 4000

ANSI_RE = re.compile(r"\x1b\[[0-9;?]*[A-Za-z]")
CTRL_RE = re.compile(r"[\x00-\x08\x0b\x0c\x0e-\x1f\x7f]")
CLIP_ID_RE = re.compile(r"gflow data download\s+([0-9a-fA-F-]{36})")
PATH_RE = re.compile(
    r"(?:[A-Za-z]:)?[\\/][^\r\n\"'<>|?*]+?\.(?:mp4|webm|mov|png|jpe?g|webp)",
    re.I)
# "unrecognized arguments: --foo", "No such option: --foo", "unknown flag: --foo"
UNKNOWN_FLAG_RE = re.compile(
    r"(?:unknown|unrecognized|unrecognised|no such|unexpected|invalid)\s+"
    r"(?:option|flag|argument)s?\W{0,3}[^\n]{0,40}?(?<![\w-])(--?[A-Za-z][\w-]*)",
    re.I)

KNOWN_ARGS = {"prompt", "text", "description", "aspect", "aspectRatio",
              "aspect_ratio", "resolution", "model", "duration", "count",
              "filename", "output_name"}
WIN_RESERVED = {"CON", "PRN", "AUX", "NUL"} | \
    {"COM%d" % i for i in range(1, 10)} | {"LPT%d" % i for i in range(1, 10)}
ASPECT_WORDS = {
    "landscape": "16:9", "horizontal": "16:9", "wide": "16:9",
    "widescreen": "16:9", "горизонтальный": "16:9", "горизонтально": "16:9",
    "portrait": "9:16", "vertical": "9:16", "tall": "9:16",
    "вертикальный": "9:16", "вертикально": "9:16",
    "square": "1:1", "квадрат": "1:1", "квадратный": "1:1",
}

# Exit codes of gflow that have a fixed meaning.
EXIT_KINDS = {14: "auth", 23: "ui_changed"}
# Failure kinds for which an automatic retry cannot help.
NO_RETRY = {"auth", "ui_changed", "quota", "policy", "profile_busy"}
# Heuristics over gflow's output (checked in this order, last 2000 chars only).
FAILURE_PATTERNS = [
    ("auth", re.compile(
        r"not logged in|log ?in required|please (?:log|sign) ?in|unauthori[sz]ed"
        r"|auth login|session (?:has )?expired|\b401\b", re.I)),
    ("profile_busy", re.compile(
        r"already in use|singleton|user data dir|profile.{0,30}(?:lock|in use)"
        r"|another (?:chrome|browser|instance)", re.I)),
    ("quota", re.compile(
        r"quota|rate.?limit|too many requests|\b429\b|resource_exhausted"
        r"|(?:limit|credits?) (?:reached|exceeded|exhausted)|out of credits", re.I)),
    ("policy", re.compile(
        r"safety|prohibited|violat|unsafe|moderation|inappropriate"
        r"|content polic|blocked (?:by|due)|not allowed", re.I)),
    ("network", re.compile(
        r"ECONN|ENOTFOUND|ETIMEDOUT|ERR_(?:INTERNET|NAME_NOT|CONNECTION|NETWORK|TIMED)"
        r"|getaddrinfo|connection (?:reset|refused|closed)|network (?:is )?unreachable"
        r"|name resolution", re.I)),
]

# How "Кредиты Flow" and "Повтор" are worded (fixed phrases = easy for an LLM).
CREDITS_NO = "не потрачены (генерация не запускалась)"
CREDITS_MAYBE = "возможно потрачены (запуск был, результата нет)"
CREDITS_SPENT = "потрачены (ролик уже создан во Flow)"
CREDITS_IMG = "не критично (повтор картинки безопасен)"
CREDITS_UNKNOWN = "неизвестно"
RETRY_FIX = "да, сразу после исправления указанного аргумента"
RETRY_ONCE = "можно один раз"
RETRY_LATER = "не сразу: подожди несколько минут"
RETRY_NO = "нет, повтор ничего не изменит"
RETRY_USER = "только после того, как пользователь устранит причину"
RETRY_ASK = "только с согласия пользователя (повтор снова тратит кредиты Flow)"

STATE = {"proc": None, "lock": None, "gate": None}
NOTES = []   # things the plugin adjusted on its own; shown on success AND failure


# ------------------------------------------------------------------ output

def reply(ok, message):
    text = str(message)
    if len(text) > MAX_REPLY:
        text = text[:MAX_REPLY - 20] + "\n...(обрезано)"
    try:
        # ensure_ascii=True (default) keeps stdout pure ASCII, so the Windows
        # console code page can never corrupt Cyrillic in the reply.
        _STDOUT.write(json.dumps({"ok": bool(ok), "message": text}) + "\n")
        _STDOUT.flush()
    except (OSError, ValueError):
        pass
    sys.exit(0)


def fail(code, problem, retry, action="", credits=CREDITS_UNKNOWN, detail=""):
    lines = ["ОШИБКА: %s." % str(problem).rstrip(". "),
             "Код: %s" % code,
             "Кредиты Flow: %s." % credits,
             "Повтор: %s." % retry]
    if action:
        lines.append("Что делать: %s" % action)
    if detail:
        lines.append("Детали: %s" % detail)
    if NOTES:
        lines.append("Примечание: %s." % "; ".join(NOTES))
    reply(False, "\n".join(lines))


def ok(prefix, files):
    text = prefix + "; ".join(str(p) for p in files[:8])
    if len(files) > 8:
        text += " (и ещё %d)" % (len(files) - 8)
    if NOTES:
        text += "\nПримечание: %s." % "; ".join(NOTES)
    reply(True, text)


def bad_arg(name, got, rule, example):
    fail("bad_argument",
         "аргумент %s=%r недопустим: %s" % (name, clip(got), rule),
         RETRY_FIX,
         "передай %s в виде %s либо вообще не передавай этот аргумент "
         "(возьмётся значение по умолчанию)" % (name, example),
         CREDITS_NO)


# ---------------------------------------------------------------- helpers

def clip(value, limit=60):
    s = CTRL_RE.sub("", str(value))
    return s if len(s) <= limit else s[:limit] + "…"


def parse_num(value):
    try:
        return int(float(str(value).strip().replace(",", ".")))
    except (TypeError, ValueError, OverflowError):
        return None


def as_int(value, default, lo=None, hi=None):
    n = parse_num(value)
    if n is None:
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


def clean_tail(text, limit=500):
    text = ANSI_RE.sub("", text or "")
    text = CTRL_RE.sub("", text.replace("\r", "\n")).strip()
    text = re.sub(r"[ \t]+", " ", text)
    text = re.sub(r"\n+", " | ", text)
    return text[-limit:]


def norm_aspect(raw):
    s = str(raw).strip().lower().replace("：", ":")
    if s in ASPECT_WORDS:
        return ASPECT_WORDS[s]
    m = re.fullmatch(r"(\d{1,2})\s*[:/x×х]\s*(\d{1,2})", s)
    if m and int(m.group(1)) > 0 and int(m.group(2)) > 0:
        return "%d:%d" % (int(m.group(1)), int(m.group(2)))
    return None


def norm_resolution(raw):
    s = str(raw).strip().lower().replace(" ", "")
    if re.fullmatch(r"\d{3,4}", s):
        s += "p"
    return s if re.fullmatch(r"\d{3,4}p|[1-8]k", s) else None


def safe_filename(raw):
    """User/LLM supplied name -> safe file name with a video extension."""
    name = re.sub(r"[^\w\-.]", "_", str(raw).strip()).strip(". ")
    if not name:
        return None
    if not name.lower().endswith(VIDEO_EXT):
        name += ".mp4"
    stem, ext = os.path.splitext(name)
    stem = stem[:80].rstrip(". ") or "video"
    if stem.upper() in WIN_RESERVED:
        stem = "_" + stem
    return stem + ext


def unique_path(out_dir, name):
    """out_dir/name, or name_2, name_3... if it already exists (never overwrite)."""
    p = out_dir / name
    if not p.exists():
        return p
    stem, ext = os.path.splitext(name)
    for i in range(2, 1000):
        q = out_dir / ("%s_%d%s" % (stem, i, ext))
        if not q.exists():
            return q
    return out_dir / ("%s_%d%s" % (stem, int(time.time()), ext))


def pid_alive(pid):
    """Is a process with this pid running? (never raises)"""
    if pid <= 0:
        return False
    if WIN:
        try:
            import ctypes
            k32 = ctypes.windll.kernel32
            h = k32.OpenProcess(0x1000, False, pid)  # QUERY_LIMITED_INFORMATION
            if not h:
                return k32.GetLastError() == 5       # access denied = it exists
            try:
                code = ctypes.c_ulong()
                if k32.GetExitCodeProcess(h, ctypes.byref(code)):
                    return code.value == 259         # STILL_ACTIVE
                return True
            finally:
                k32.CloseHandle(h)
        except Exception:
            return True
    try:
        os.kill(pid, 0)  # POSIX only: on Windows this would KILL the process
    except ProcessLookupError:
        return False
    except OSError:
        return True
    return True


def kill_tree(proc):
    """Kill gflow together with the Chrome it started."""
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


def run_cmd(cmd, env, timeout):
    """Returns (returncode | None on timeout, stdout, stderr, timed_out).
    list argv, no shell: the prompt comes from the LLM / chat users."""
    kw = {}
    if WIN:
        kw["creationflags"] = CREATE_NO_WINDOW
    else:
        kw["start_new_session"] = True
    proc = subprocess.Popen(cmd, stdin=subprocess.DEVNULL,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                            text=True, encoding="utf-8", errors="replace",
                            env=env, **kw)
    STATE["proc"] = proc
    lock = STATE.get("lock")
    if lock is not None:
        lock.set_child(proc.pid)
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
    finally:
        STATE["proc"] = None
        if lock is not None:
            lock.set_child(None)


def cleanup():
    """Called when the host stops us: no orphan gflow/Chrome, no stuck lock."""
    proc = STATE.get("proc")
    try:
        if proc is not None and proc.poll() is None:
            kill_tree(proc)
    except Exception:
        pass
    for key, method in (("gate", "drop"), ("lock", "release")):
        obj = STATE.get(key)
        try:
            if obj is not None:
                getattr(obj, method)()
        except Exception:
            pass


def install_signal_handlers():
    def on_signal(signum, frame):
        cleanup()
        fail("cancelled", "плагин остановлен приложением до завершения генерации",
             RETRY_ASK, "сообщи пользователю, что генерация прервана; "
             "не запускай её заново без его согласия", CREDITS_MAYBE)
    for name in ("SIGTERM", "SIGINT", "SIGBREAK", "SIGHUP"):
        sig = getattr(signal, name, None)
        if sig is None:
            continue
        try:
            signal.signal(sig, on_signal)
        except (ValueError, OSError):
            pass


def exe_fingerprint(gflow):
    """Changes after `gflow update`, so the flag cache never goes stale."""
    path = shutil.which(gflow)
    if not path:
        return gflow
    try:
        st = os.stat(path)
        return "%s:%d:%d" % (path, int(st.st_mtime), st.st_size)
    except OSError:
        return path


def supported_flags(gflow, sub, env, data_dir):
    """Asks `gflow <sub> --help` which --options exist (cached for a day;
    a failed probe is cached for 2 minutes so a hanging --help costs 45 s once).
    Returns a set of flags, or None when it could not be determined."""
    cache_file = data_dir / "flags_cache.json"
    key = "%s|%s" % (exe_fingerprint(gflow), " ".join(sub))
    try:
        cache = json.loads(cache_file.read_text(encoding="utf-8"))
        if not isinstance(cache, dict):
            cache = {}
    except Exception:
        cache = {}
    entry = cache.get(key)
    if isinstance(entry, dict):
        try:
            age = time.time() - float(entry.get("ts", 0))
        except (TypeError, ValueError):
            age = 1e9
        if entry.get("flags") and age < 86400:
            return set(entry["flags"])
        if entry.get("failed") and age < 120:
            return None

    def store(value):
        cache[key] = value
        tmp = cache_file.with_name("flags_cache.%d.tmp" % os.getpid())
        try:
            tmp.write_text(json.dumps(cache), encoding="utf-8")
            os.replace(str(tmp), str(cache_file))   # atomic: no half-written cache
        except OSError:
            try:
                tmp.unlink()
            except OSError:
                pass

    help_env = dict(env, COLUMNS="250", NO_COLOR="1", TERM="dumb")
    try:
        code, out, err, timed_out = run_cmd([gflow] + sub + ["--help"],
                                            help_env, 45)
    except OSError:
        return None
    flags = set(re.findall(r"(?<![\w-])(--[a-z][a-z0-9-]*)",
                           ANSI_RE.sub("", out + "\n" + err)))
    if timed_out or code != 0 or not flags:
        store({"ts": time.time(), "failed": True})
        return None
    store({"ts": time.time(), "flags": sorted(flags)})
    return flags


def add_opt(cmd, flags, flag, value, ignored, send_if_unknown=False):
    """Adds `flag value` only if this gflow version supports the flag."""
    if value in (None, ""):
        return
    if (flags is None and send_if_unknown) or (flags is not None and flag in flags):
        cmd += [flag, str(value)]
    else:
        ignored.append(flag)


def snapshot(out_dir):
    """path -> (mtime_ns, size): lets us notice files that gflow OVERWROTE."""
    snap = {}
    try:
        for q in out_dir.rglob("*"):
            try:
                if q.is_file():
                    st = q.stat()
                    snap[q] = (st.st_mtime_ns, st.st_size)
            except OSError:
                continue
    except OSError:
        pass
    return snap


def list_new_files(out_dir, before):
    found = []
    try:
        for q in out_dir.rglob("*"):
            try:
                if not q.is_file() or q.suffix.lower() not in MEDIA_EXT:
                    continue
                st = q.stat()
                if st.st_size > 0 and before.get(q) != (st.st_mtime_ns, st.st_size):
                    found.append((st.st_mtime, q))
            except OSError:
                continue
    except OSError:
        pass
    return [q for _, q in sorted(found)]


def paths_from_text(text):
    found = []
    for m in PATH_RE.finditer(ANSI_RE.sub("", text or "")):
        p = Path(m.group(0).strip())
        try:
            if p.is_file() and p.stat().st_size > 0:
                found.append(p)
        except OSError:
            continue
    return found


def merge_files(*groups):
    seen, result = set(), []
    for group in groups:
        for p in group:
            if str(p) not in seen:
                seen.add(str(p))
                result.append(p)
    return result


def classify(code, out, err, timed_out):
    """What kind of failure is this? Heuristic, so the wording says 'похоже'."""
    if timed_out:
        return "timeout"
    if code in EXIT_KINDS:
        return EXIT_KINDS[code]
    text = ANSI_RE.sub("", (err or "") + "\n" + (out or ""))[-2000:]
    for kind, rx in FAILURE_PATTERNS:
        if rx.search(text):
            return kind
    return "unknown"


# ------------------------------------------------------------ queue + lock

class Lock:
    """One gflow at a time: it uses a single persistent browser profile.
    The lock file holds our pid and the pid of the running gflow, so a lock
    left by a dead process is recognised at once (not after an hour)."""

    def __init__(self, path, stale_s):
        self.path, self.stale_s, self.held = Path(path), stale_s, False

    def _stale(self):
        try:
            age = time.time() - os.path.getmtime(str(self.path))
            raw = self.path.read_text(encoding="utf-8", errors="replace")
        except OSError:
            return False
        if age > self.stale_s:
            return True
        pids = [int(x) for x in re.findall(r"\d+", raw)]
        if not pids:
            return age > 30          # empty: maybe being created right now
        return not any(pid_alive(p) for p in pids)

    def acquire(self):
        for _ in range(3):
            try:
                fd = os.open(str(self.path), os.O_CREAT | os.O_EXCL | os.O_WRONLY)
            except FileExistsError:
                if self._stale():
                    try:
                        os.remove(str(self.path))
                    except OSError:
                        pass
                    continue
                return False
            except OSError:
                return True  # cannot lock for some reason: do not block work
            try:
                os.write(fd, ("%d\n" % os.getpid()).encode())
            except OSError:
                pass
            finally:
                os.close(fd)
            self.held = True
            return True
        return False

    def set_child(self, pid):
        """Record the running gflow (also refreshes the lock's mtime)."""
        if not self.held:
            return
        try:
            self.path.write_text("%d\n%s\n" % (os.getpid(), pid or ""),
                                 encoding="utf-8")
        except OSError:
            pass

    def release(self):
        if self.held:
            try:
                os.remove(str(self.path))
            except OSError:
                pass
            self.held = False


class Gate:
    """FIFO queue in front of the Lock. Every waiting call keeps a ticket file
    (touched every 2 s); the oldest live ticket goes first. A ticket of a dead
    process, or one not touched for 60 s, is dropped."""

    def __init__(self, data_dir, lock):
        self.qdir = data_dir / "queue"
        self.lock = lock
        self.ticket = None

    def _live(self):
        now, names = time.time(), []
        try:
            for t in self.qdir.glob("*.t"):
                try:
                    dead = now - t.stat().st_mtime > 60
                    if not dead:
                        m = re.search(r"_(\d+)\.t$", t.name)
                        if m and int(m.group(1)) != os.getpid():
                            dead = not pid_alive(int(m.group(1)))
                    if dead:
                        t.unlink()
                        continue
                except OSError:
                    continue
                names.append(t.name)
        except OSError:
            pass
        return sorted(names)

    def drop(self):
        if self.ticket is not None:
            try:
                self.ticket.unlink()
            except OSError:
                pass

    def wait(self, max_wait_s):
        try:
            self.qdir.mkdir(parents=True, exist_ok=True)
            self.ticket = self.qdir / ("%020d_%d.t" % (time.time_ns(), os.getpid()))
            self.ticket.write_text("", encoding="utf-8")
        except OSError:
            self.ticket = None  # no queue dir: plain lock without ordering
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
            self.drop()


# ------------------------------------------------------------------ main

def normalize_function(fn):
    key = re.sub(r"[^a-z]", "", str(fn or "").lower())
    return {"generateimage": "generateImage",
            "generatevideo": "generateVideo"}.get(key)


def coerce_args(req):
    """LLMs get this wrong in many ways: args as a JSON string, as a bare
    prompt string, or placed next to "function" instead of inside "args"."""
    args = req.get("args")
    if isinstance(args, str):
        try:
            parsed = json.loads(args)
        except ValueError:
            parsed = None
        if isinstance(parsed, dict):
            args = parsed
        else:
            args = {"prompt": args} if args.strip() else {}
        NOTES.append("args пришёл строкой, а не объектом JSON: передавай args объектом")
    if not isinstance(args, dict):
        args = {}
    if not args:
        args = {k: v for k, v in req.items()
                if k not in ("function", "args", "context")}
        if args:
            NOTES.append("аргументы найдены рядом с function, а не внутри args: "
                         "в следующий раз клади их внутрь args")
    return args


def handle(req):
    fn = normalize_function(req.get("function"))
    if fn is None:
        fail("unknown_function",
             "неизвестная функция %r" % clip(req.get("function")), RETRY_FIX,
             "вызывай только generateImage (картинки) или generateVideo (видео)",
             CREDITS_NO)
    is_video = fn == "generateVideo"

    args = coerce_args(req)
    ctx = req.get("context")
    ctx = ctx if isinstance(ctx, dict) else {}
    cfg = ctx.get("settings")
    cfg = cfg if isinstance(cfg, dict) else {}

    if is_video and not as_bool(cfg.get("allow_video"), False):
        fail("video_disabled",
             "генерация видео выключена пользователем (allow_video=false в config.ini)",
             RETRY_NO,
             "не пытайся обойти запрет; сообщи пользователю, что видео отключено, "
             "и предложи либо включить allow_video в настройках плагина, либо "
             "сделать картинки через generateImage",
             CREDITS_NO)

    gflow = os.path.expanduser(
        str(cfg.get("gflow_path") or "gflow").strip().strip("\"'"))
    if is_video:
        timeout = as_int(cfg.get("video_timeout_s"), 600, 60, 3600)
    else:
        timeout = as_int(cfg.get("timeout_s"), 280, 30, 3600)

    # ---- where to put files
    data_dir = ctx.get("dataDir")
    if data_dir:
        data_dir = Path(str(data_dir))
    else:
        data_dir = Path(__file__).resolve().parent / "_data"
        NOTES.append("приложение не передало dataDir, файлы сохраняются в %s"
                     % (data_dir / "out"))
    out_dir = data_dir / "out"
    try:
        out_dir.mkdir(parents=True, exist_ok=True)
        probe = out_dir / (".probe_%d" % os.getpid())
        probe.write_text("x", encoding="utf-8")
        probe.unlink()
    except OSError as e:
        fail("out_dir_error", "нет доступа на запись в папку результатов %s" % out_dir,
             RETRY_USER,
             "это проблема окружения, а не аргументов: сообщи пользователю, "
             "чтобы проверил права на папку и свободное место",
             CREDITS_NO, "%s: %s" % (type(e).__name__, e))

    # ---- argument validation (nothing here costs credits)
    prompt = ""
    for key in ("prompt", "text", "description"):
        if args.get(key) not in (None, ""):
            prompt = str(args.get(key))
            break
    prompt = CTRL_RE.sub(" ", prompt).strip()
    stripped = prompt.lstrip("-–—").strip()   # a leading dash would look like an option
    if stripped != prompt:
        NOTES.append("ведущие дефисы в prompt убраны")
        prompt = stripped
    if not prompt:
        fail("bad_argument", "аргумент prompt пустой или отсутствует", RETRY_FIX,
             "передай prompt — текстовое описание того, что нужно сгенерировать",
             CREDITS_NO)
    if len(prompt) > MAX_PROMPT:
        fail("prompt_too_long",
             "prompt слишком длинный: %d символов при максимуме %d"
             % (len(prompt), MAX_PROMPT), RETRY_FIX,
             "сократи описание до %d символов (оставь главное: объект, действие, "
             "стиль, свет) и вызови функцию снова" % MAX_PROMPT, CREDITS_NO)

    raw = args.get("aspect") or args.get("aspectRatio") or args.get("aspect_ratio")
    if raw not in (None, ""):
        aspect = norm_aspect(raw)
        if aspect is None:
            bad_arg("aspect", raw, "ожидается соотношение сторон", "16:9, 9:16 или 1:1")
    else:
        aspect = norm_aspect(cfg.get("default_aspect") or "16:9") or "16:9"

    raw = args.get("resolution")
    if raw not in (None, ""):
        resolution = norm_resolution(raw)
        if resolution is None:
            bad_arg("resolution", raw, "ожидается число с p или k", "720p, 1080p или 4k")
    else:
        resolution = norm_resolution(cfg.get("default_resolution") or "") or ""

    model = str(args.get("model") or "").strip()
    if model and not re.fullmatch(r"[A-Za-z0-9._-]{1,40}", model):
        bad_arg("model", model, "допустимы только латинские буквы, цифры, . _ - "
                "(до 40 символов)", "имя модели без пробелов")

    duration = None
    if args.get("duration") not in (None, ""):
        if is_video:
            m = re.fullmatch(r"(\d{1,3}(?:[.,]\d+)?)\s*(?:s|secs?|seconds?|с|сек\w*)?",
                             str(args.get("duration")).strip().lower())
            if not m:
                bad_arg("duration", args.get("duration"),
                        "ожидается число секунд", "6")
            want = int(round(float(m.group(1).replace(",", "."))))
            max_dur = as_int(cfg.get("max_duration_s"), 8, 1, 120)
            duration = max(1, min(want, max_dur))
            if duration != want:
                NOTES.append("duration=%d изменена на %d (лимит max_duration_s=%d)"
                             % (want, duration, max_dur))

    count_cap = (as_int(cfg.get("max_video_count"), 1, 1, 4) if is_video else 4)
    count = 1
    if args.get("count") not in (None, ""):
        n = parse_num(args.get("count"))
        if n is None:
            NOTES.append("count=%s не число, создаётся 1 файл" % clip(args.get("count")))
        else:
            count = max(1, min(n, count_cap))
            if count != n:
                NOTES.append("count=%d изменён на %d (максимум за один вызов: %d%s)"
                             % (n, count, count_cap,
                                ", настройка max_video_count" if is_video else ""))

    ignored = []
    extra = sorted(str(k) for k in args if k not in KNOWN_ARGS)
    if extra:
        NOTES.append("неизвестные аргументы проигнорированы: %s" % ", ".join(extra))
    if args.get("duration") not in (None, "") and not is_video:
        ignored.append("duration (только для видео)")
    raw_name = str(args.get("filename") or args.get("output_name") or "").strip()
    out_name = safe_filename(raw_name) if (is_video and raw_name) else None
    if raw_name and not is_video:
        ignored.append("filename (только для видео)")
    if raw_name and is_video and count > 1:
        out_name = None
        ignored.append("filename (при count > 1 не применяется)")

    # ---- environment checks (still free)
    if not shutil.which(gflow):
        fail("gflow_not_found", "не найден исполняемый файл gflow (gflow_path=%r)" % gflow,
             RETRY_USER,
             "сообщи пользователю: установить gflow или указать полный путь к "
             "gflow / gflow.exe в параметре gflow_path (config.ini, секция этого "
             "плагина), затем повторить", CREDITS_NO)
    try:
        free_mb = shutil.disk_usage(str(out_dir)).free // (1024 * 1024)
        need_mb = 300 if is_video else 50
        if free_mb < need_mb:
            fail("disk_full", "на диске мало места: свободно %d МБ, нужно хотя бы %d МБ"
                 % (free_mb, need_mb), RETRY_USER,
                 "сообщи пользователю, чтобы освободил место на диске с %s" % out_dir,
                 CREDITS_NO)
    except OSError:
        pass

    env = dict(os.environ)
    env["GFLOW_CLI_OUTPUT_DIR"] = str(out_dir)
    env["PYTHONUTF8"] = "1"            # gflow prints unicode into our pipes
    env["PYTHONIOENCODING"] = "utf-8"
    env.setdefault("NO_COLOR", "1")

    sub = ["video", "t2v"] if is_video else ["image", "t2i"]
    flags = supported_flags(gflow, sub, env, data_dir)

    # ---- queue: wait for our turn instead of failing when gflow is busy
    lock = Lock(data_dir / "gflow.lock", timeout + 120)
    gate = Gate(data_dir, lock)
    STATE["lock"], STATE["gate"] = lock, gate
    wait_s = as_int(cfg.get("queue_wait_s"), 3600, 0, 86400)
    if not gate.wait(wait_s):
        fail("queue_timeout",
             "очередь gflow не освободилась за %d с: предыдущие задачи ещё идут" % wait_s,
             RETRY_LATER,
             "сообщи пользователю, что генерация занята; не запускай несколько "
             "вызовов подряд — они встают в одну очередь", CREDITS_NO)

    started = False
    try:
        # ---- pause between two video generations (counted from the last end)
        if is_video:
            gap = as_int(cfg.get("video_min_interval_s"), 90, 0, 3600)
            if gap:
                try:
                    last = float((data_dir / "last_video.txt")
                                 .read_text(encoding="utf-8").strip())
                except (OSError, ValueError):
                    last = 0.0
                pause = last + gap - time.time()
                if pause > 0:
                    time.sleep(min(pause, gap))

        # ---- build the command, using only options this gflow understands
        out_path = None
        cmd = [gflow] + sub + [prompt]
        add_opt(cmd, flags, "--aspect", aspect, ignored, send_if_unknown=True)
        add_opt(cmd, flags, "--model", model, ignored)
        if resolution:
            rflag = next((f for f in ("--resolution", "--res", "--quality")
                          if flags and f in flags), None)
            if rflag:
                cmd += [rflag, resolution]
            else:
                ignored.append("resolution")
        add_opt(cmd, flags, "--duration", duration if is_video else None, ignored)
        add_opt(cmd, flags, "--count", count if count > 1 else None, ignored)
        if is_video and count == 1:
            out_path = unique_path(
                out_dir, out_name or ("video_%d.mp4" % int(time.time())))
            if raw_name and out_path.name != raw_name:
                NOTES.append("имя файла: запрошено %s, сохранено как %s "
                             "(такое имя занято или недопустимо)" % (raw_name, out_path.name))
            cmd += ["-o", str(out_path)]
        if ignored:
            NOTES.append("не применены параметры: %s (эта версия gflow их не "
                         "поддерживает или они не нужны для этой функции)"
                         % ", ".join(ignored))

        started = True
        run(cmd, gflow, env, timeout, out_dir, out_path, is_video,
            as_int(cfg.get("image_retries"), 1, 0, 3), count)
    finally:
        if is_video and started:
            try:
                (data_dir / "last_video.txt").write_text(str(time.time()),
                                                         encoding="utf-8")
            except OSError:
                pass
        lock.release()


def explain_failure(kind, code, tail, is_video, attempts):
    credits = CREDITS_MAYBE if is_video else CREDITS_IMG
    once = RETRY_ASK if is_video else RETRY_ONCE
    ask = " и только с согласия пользователя (повтор тратит кредиты Flow)" if is_video else ""
    tries = " (попыток: %d)" % attempts if attempts > 1 else ""
    detail = ("вывод gflow: " + tail) if tail else "gflow ничего не вывел"

    if kind == "auth":
        fail("auth_required",
             "gflow не авторизован в Google: нужен вход или сессия истекла", RETRY_USER,
             "сообщи пользователю, что один раз нужно выполнить в терминале "
             "`gflow auth login --browser chrome`, после этого повторить запрос; "
             "сам ты это исправить не можешь", CREDITS_NO, detail)
    if kind == "ui_changed":
        fail("flow_ui_changed",
             "интерфейс Google Flow изменился, эта версия gflow с ним не работает",
             RETRY_USER,
             "сообщи пользователю: выполнить `gflow update`, затем повторить; "
             "пока gflow не обновлён, повтор бесполезен", credits, detail)
    if kind == "quota":
        fail("quota_or_rate_limit",
             "похоже, исчерпан лимит или кредиты Flow, либо запросы идут слишком часто",
             RETRY_LATER + ask,
             "не повторяй сразу: сообщи пользователю и предложи подождать или "
             "проверить лимиты и баланс во Flow", credits, detail)
    if kind == "policy":
        fail("content_rejected",
             "похоже, Flow отклонил запрос фильтром безопасности (из-за prompt)",
             "только с другим prompt: тот же текст отклонят снова" + ask,
             "перепиши prompt: убери реальных людей и знаменитостей, бренды и "
             "персонажей под авторским правом, насилие, откровенное содержание; "
             "опиши сцену нейтрально и вызови снова"
             + (" (новый текст сначала покажи пользователю)" if is_video else ""),
             credits, detail)
    if kind == "profile_busy":
        fail("profile_busy",
             "профиль браузера gflow занят: скорее всего открыт Chrome с этим же "
             "профилем или остался зависший процесс", RETRY_USER,
             "сообщи пользователю: закрыть окна Chrome, открытые gflow (при "
             "необходимости завершить зависший chrome/gflow в диспетчере задач), "
             "затем повторить", credits, detail)
    if kind == "network":
        fail("network_error", "сбой сети при обращении gflow к Google" + tries, once,
             "если повтор не помог — сообщи пользователю, чтобы проверил интернет и VPN",
             credits, detail)
    if kind == "no_file":
        fail("no_output_file",
             "gflow завершился без ошибки, но файла результата нет" + tries, once,
             "возможно, Flow ничего не вернул или gflow изменил формат вывода; если "
             "повтор не помог — сообщи пользователю и покажи детали ниже дословно",
             credits, detail)
    fail("gflow_failed", "gflow завершился с кодом %s%s" % (code, tries), once,
         "причина по выводу не определена: не выдумывай её; если повтор не помог — "
         "сообщи пользователю текст из деталей дословно", credits, detail)


def run(cmd, gflow, env, timeout, out_dir, out_path, is_video, retries, want_count):
    before = snapshot(out_dir)

    def fix_names(files):
        """Make sure the video ends up under the requested name, even if this
        gflow version ignored -o and saved it under its own name."""
        if out_path is None:
            return files
        if not out_path.exists():
            cand = [p for p in files
                    if p.suffix.lower() in VIDEO_EXT
                    and str(p).startswith(str(out_dir))]
            if cand:
                try:
                    os.replace(str(cand[-1]), str(out_path))
                    files = [p for p in files if p != cand[-1]]
                except OSError:
                    return files
        if out_path.exists():
            files = [out_path] + [p for p in files if p != out_path]
        return files

    attempts = 1 if is_video else 1 + retries
    attempt = pruned = 0
    code, out, err, timed_out, files = None, "", "", False, []
    kind = "unknown"
    while True:
        attempt += 1
        try:
            code, out, err, timed_out = run_cmd(cmd, env, timeout)
        except FileNotFoundError:
            fail("gflow_not_found", "не удалось запустить gflow: файл не найден",
                 RETRY_USER, "сообщи пользователю: проверить gflow_path в config.ini "
                 "или установить gflow", CREDITS_NO)
        except OSError as e:
            fail("gflow_launch_failed", "не удалось запустить gflow", RETRY_USER,
                 "сообщи пользователю: проверить, что gflow_path указывает на рабочий "
                 "gflow, а не на повреждённый файл или папку", CREDITS_NO,
                 "%s: %s" % (type(e).__name__, e))
        files = merge_files(list_new_files(out_dir, before),
                            paths_from_text(out + "\n" + err))
        if code == 0 and files:
            break
        if timed_out:
            kind = "timeout"
            break
        # This gflow rejected one of our options: drop it and run again. gflow
        # checks options before it starts working, so this costs no credits.
        m = UNKNOWN_FLAG_RE.search(ANSI_RE.sub("", err + "\n" + out))
        if m and m.group(1) in cmd and pruned < 4:
            flag = m.group(1)
            i = cmd.index(flag)
            del cmd[i:i + 2]
            pruned += 1
            NOTES.append("gflow не принял параметр %s, он пропущен" % flag)
            attempt -= 1
            continue
        kind = classify(code, out, err, False)
        if kind in NO_RETRY or attempt >= attempts:
            break
        time.sleep(3 * attempt)

    # 1) success
    if code == 0 and files:
        if attempt > 1:
            NOTES.append("получилось с %d-й попытки" % attempt)
        files = fix_names(files)
        if out_path is not None and files[0] != out_path:
            NOTES.append("не удалось переименовать файл в %s, он лежит по пути выше"
                         % out_path.name)
        if len(files) < want_count:
            NOTES.append("запрошено файлов: %d, получено: %d" % (want_count, len(files)))
        ok("Готово, файл: ", files)

    tail = clean_tail(err or out)

    # 2) the clip was generated but not downloaded: fetch it by id instead of
    #    generating (and paying for) it again
    clip_m = CLIP_ID_RE.search(ANSI_RE.sub("", out + "\n" + err))
    if clip_m:
        cid = clip_m.group(1)
        for dl in range(2):
            try:
                _c, o2, e2, _t = run_cmd([gflow, "data", "download", cid],
                                         env, min(timeout, 240))
            except OSError:
                break
            got = merge_files(list_new_files(out_dir, before),
                              paths_from_text(o2 + "\n" + e2))
            if got:
                ok("Готово (скачано повторно по id клипа), файл: ", fix_names(got))
            if dl == 0:
                time.sleep(5)
        fail("download_failed", "ролик создан во Flow, но файл не скачался", RETRY_NO,
             "НЕ генерируй заново (это снова спишет кредиты): сообщи пользователю "
             "и предложи скачать вручную командой `gflow data download %s`" % cid,
             CREDITS_SPENT, "вывод gflow: " + tail if tail else "")

    # 3) gflow did not finish cleanly, but a new media file did appear
    new_files = list_new_files(out_dir, before)
    if new_files:
        new_files = fix_names(new_files)
        why = ("остановлен по таймауту (%d с)" % timeout if timed_out
               else "завершился с кодом %s" % code)
        if is_video:
            fail("partial_result",
                 "gflow %s, но в папке появился файл %s: видео может быть неполным"
                 % (why, new_files[0]), RETRY_ASK,
                 "проверь файл (размер, воспроизводится ли) до использования; "
                 "пока пользователь не разрешил, не запускай генерацию заново",
                 CREDITS_MAYBE, "вывод gflow: " + tail if tail else "")
        NOTES.append("gflow %s, поэтому часть запрошенных файлов могла не создаться" % why)
        ok("Готово (частично), файл: ", new_files)

    # 4) timeout
    if timed_out:
        if is_video:
            fail("timeout", "gflow не уложился в %d с и был остановлен" % timeout,
                 RETRY_ASK,
                 "ролик мог дойти до Flow и создаться там: сообщи пользователю и "
                 "предложи сначала проверить проект во Flow; если нужен повтор, "
                 "можно увеличить video_timeout_s в config.ini",
                 CREDITS_MAYBE, "вывод gflow: " + tail if tail else "")
        fail("timeout", "gflow не уложился в %d с и был остановлен" % timeout,
             RETRY_ONCE,
             "повтори один раз (можно с меньшим count); если снова таймаут — "
             "сообщи пользователю: Flow перегружен или нужно увеличить timeout_s "
             "в config.ini", CREDITS_IMG, "вывод gflow: " + tail if tail else "")

    # 5) every other failure
    if code == 0 and kind == "unknown":
        kind = "no_file"
    explain_failure(kind, code, tail, is_video, attempt)


def main():
    install_signal_handlers()
    try:
        # Qt writes UTF-8; do not rely on the locale encoding of sys.stdin.
        raw = sys.stdin.buffer.read()
        req = json.loads(raw.decode("utf-8-sig", errors="replace"))
        if not isinstance(req, dict):
            raise ValueError("ожидался JSON-объект")
    except Exception as e:
        fail("bad_request", "приложение передало плагину нечитаемый запрос", RETRY_NO,
             "это сбой приложения, а не твоих аргументов: не повторяй вызов, "
             "сообщи пользователю", CREDITS_NO, "%s: %s" % (type(e).__name__, e))
    try:
        handle(req)
    except Exception as e:  # SystemExit from reply() is not caught here
        where = ""
        try:
            fr = traceback.extract_tb(e.__traceback__)[-1]
            where = " (строка %d, %s)" % (fr.lineno, fr.name)
        except Exception:
            pass
        cleanup()
        fail("plugin_bug", "внутренняя ошибка плагина gflow", RETRY_NO,
             "это баг плагина, а не твоих аргументов: не повторяй вызов, сообщи "
             "пользователю текст из деталей дословно",
             CREDITS_UNKNOWN, "%s: %s%s" % (type(e).__name__, e, where))


if __name__ == "__main__":
    main()