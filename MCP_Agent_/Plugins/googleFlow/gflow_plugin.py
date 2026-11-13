#!/usr/bin/env python3
"""Plugin 'gflow': drives gflow-cli (unofficial Google Flow client).

Protocol: one JSON object on stdin {function, args, context},
one JSON object on stdout {"ok": bool, "message": str}.
Nothing else may be printed to stdout.

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

Arguments: prompt (required), aspect, model, duration (video), count.
"""
import json
import os
import re
import signal
import subprocess
import sys
import time
from pathlib import Path

WIN = sys.platform == "win32"
CREATE_NO_WINDOW = 0x08000000
MEDIA_EXT = (".mp4", ".webm", ".mov", ".png", ".jpg", ".jpeg", ".webp")
ANSI_RE = re.compile(r"\x1b\[[0-9;?]*[A-Za-z]")
CLIP_ID_RE = re.compile(r"gflow data download\s+([0-9a-fA-F-]{36})")
PATH_RE = re.compile(
    r"(?:[A-Za-z]:)?[\\/][^\r\n\"'<>|?*]+?\.(?:mp4|webm|mov|png|jpe?g|webp)",
    re.I)
EXIT_HINTS = {
    14: "нужен вход в Google: выполните в терминале "
        "gflow auth login --browser chrome",
    23: "интерфейс Flow изменился: обновите gflow командой gflow update",
}
VIDEO_WARN = (" Не повторяй вызов автоматически: генерация видео тратит "
              "кредиты Flow. Сначала сообщи пользователю.")


def reply(ok, message):
    # ensure_ascii=True (default) keeps stdout pure ASCII, so the Windows
    # console code page can never corrupt Cyrillic in the reply.
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


def as_bool(value, default=False):
    if value is None:
        return default
    return str(value).strip().lower() in ("1", "true", "yes", "on")


def clean_tail(text, limit=500):
    text = ANSI_RE.sub("", text or "").strip()
    text = re.sub(r"[ \t]+", " ", text)
    return text[-limit:]


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


def supported_flags(gflow, sub, env, data_dir):
    """Asks `gflow <sub> --help` which --options exist (cached for a day).
    Returns a set of flags, or None when it could not be determined."""
    cache_file = data_dir / "flags_cache.json"
    key = "%s|%s" % (gflow, " ".join(sub))
    try:
        cache = json.loads(cache_file.read_text(encoding="utf-8"))
        if not isinstance(cache, dict):
            cache = {}
    except Exception:
        cache = {}
    entry = cache.get(key)
    if isinstance(entry, dict) and time.time() - entry.get("ts", 0) < 86400:
        return set(entry.get("flags", []))

    help_env = dict(env, COLUMNS="250", NO_COLOR="1", TERM="dumb")
    try:
        code, out, err, timed_out = run_cmd([gflow] + sub + ["--help"],
                                            help_env, 45)
    except OSError:
        return None
    flags = set(re.findall(r"(?<![\w-])(--[a-z][a-z0-9-]*)",
                           ANSI_RE.sub("", out + "\n" + err)))
    if timed_out or code != 0 or not flags:
        return None
    cache[key] = {"ts": time.time(), "flags": sorted(flags)}
    try:
        cache_file.write_text(json.dumps(cache), encoding="utf-8")
    except OSError:
        pass
    return flags


def add_opt(cmd, flags, flag, value, ignored, send_if_unknown=False):
    """Adds `flag value` only if this gflow version supports the flag."""
    if value in (None, ""):
        return
    if (flags is None and send_if_unknown) or (flags is not None and flag in flags):
        cmd += [flag, str(value)]
    else:
        ignored.append(flag)


def list_new_files(out_dir, before):
    found = []
    try:
        for q in out_dir.rglob("*"):
            try:
                if (q.is_file() and q not in before
                        and q.suffix.lower() in MEDIA_EXT
                        and q.stat().st_size > 0):
                    found.append((q.stat().st_mtime, q))
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


class Lock:
    """One gflow at a time: it uses a single persistent browser profile."""

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
                return True  # cannot lock for some reason: do not block work
        return False

    def release(self):
        if self.held:
            try:
                os.remove(str(self.path))
            except OSError:
                pass
            self.held = False


# ------------------------------------------------------------------ main

def handle(req):
    fn = req.get("function")
    args = req.get("args")
    ctx = req.get("context")
    args = args if isinstance(args, dict) else {}
    ctx = ctx if isinstance(ctx, dict) else {}
    cfg = ctx.get("settings")
    cfg = cfg if isinstance(cfg, dict) else {}

    if fn not in ("generateImage", "generateVideo"):
        reply(False, "неизвестная функция " + str(fn))
    is_video = fn == "generateVideo"

    gflow = str(cfg.get("gflow_path") or "gflow").strip()
    if is_video:
        timeout = as_int(cfg.get("video_timeout_s"), 600, 60, 3600)
    else:
        timeout = as_int(cfg.get("timeout_s"), 280, 30, 3600)

    data_dir = ctx.get("dataDir")
    if not data_dir:
        reply(False, "в context нет dataDir")
    data_dir = Path(str(data_dir))
    out_dir = data_dir / "out"
    try:
        out_dir.mkdir(parents=True, exist_ok=True)
    except OSError as e:
        reply(False, "не удалось создать папку вывода: %s" % e)

    # ---- argument validation (nothing here costs credits)
    prompt = str(args.get("prompt", "")).strip()
    if not prompt or len(prompt) > 2000 or prompt.startswith("-"):
        reply(False, "пустой, слишком длинный или недопустимый prompt")

    aspect = str(args.get("aspect") or cfg.get("default_aspect") or "16:9").strip()
    if not re.fullmatch(r"\d{1,2}:\d{1,2}", aspect):
        reply(False, "недопустимый aspect, ожидается вид 16:9")

    model = str(args.get("model") or "").strip()
    if model and not re.fullmatch(r"[A-Za-z0-9._-]{1,40}", model):
        reply(False, "недопустимое имя модели")

    duration = None
    if args.get("duration") not in (None, ""):
        m = re.fullmatch(r"(\d{1,3})\s*(?:s|с|sec)?",
                         str(args.get("duration")).strip().lower())
        if not m:
            reply(False, "недопустимая duration, ожидается число секунд")
        max_dur = as_int(cfg.get("max_duration_s"), 8, 1, 120)
        duration = max(1, min(int(m.group(1)), max_dur))

    count_cap = (as_int(cfg.get("max_video_count"), 1, 1, 4) if is_video else 4)
    count = as_int(args.get("count"), 1, 1, count_cap)

    if is_video:
        if not as_bool(cfg.get("allow_video"), False):
            reply(False, "генерация видео отключена настройкой allow_video")
        gap = as_int(cfg.get("video_min_interval_s"), 90, 0, 86400)
        stamp = data_dir / "last_video.txt"
        if gap:
            try:
                age = time.time() - float(stamp.read_text(encoding="utf-8"))
                if 0 <= age < gap:
                    reply(False, "видео уже запускали %d с назад; подождите "
                                 "ещё %d с, чтобы не тратить кредиты зря"
                                 % (age, gap - age))
            except (OSError, ValueError):
                pass

    env = dict(os.environ)
    env["GFLOW_CLI_OUTPUT_DIR"] = str(out_dir)
    env["PYTHONUTF8"] = "1"            # gflow prints unicode into our pipes
    env["PYTHONIOENCODING"] = "utf-8"

    # ---- build the command, using only options this gflow understands
    sub = ["video", "t2v"] if is_video else ["image", "t2i"]
    flags = supported_flags(gflow, sub, env, data_dir)
    ignored = []
    cmd = [gflow] + sub + [prompt]
    add_opt(cmd, flags, "--aspect", aspect, ignored, send_if_unknown=True)
    add_opt(cmd, flags, "--model", model, ignored)
    add_opt(cmd, flags, "--duration", duration if is_video else None, ignored)
    add_opt(cmd, flags, "--count", count if count > 1 else None, ignored)
    if is_video:
        cmd += ["-o", str(out_dir / ("video_%d.mp4" % int(time.time())))]
    elif duration is not None:
        ignored.append("duration (только для видео)")

    lock = Lock(data_dir / "gflow.lock", timeout + 120)
    if not lock.acquire():
        reply(False, "gflow занят другой генерацией, повторите через минуту")
    try:
        run(cmd, gflow, env, timeout, out_dir, is_video, ignored, data_dir,
            as_int(cfg.get("image_retries"), 1, 0, 3))
    finally:
        lock.release()


def run(cmd, gflow, env, timeout, out_dir, is_video, ignored, data_dir,
        retries):
    before = {p for p in out_dir.rglob("*") if p.is_file()}
    if is_video:
        try:
            (data_dir / "last_video.txt").write_text(str(time.time()),
                                                     encoding="utf-8")
        except OSError:
            pass

    attempts = 1 if is_video else 1 + retries
    code, out, err, timed_out, files = None, "", "", False, []
    for attempt in range(attempts):
        try:
            code, out, err, timed_out = run_cmd(cmd, env, timeout)
        except FileNotFoundError:
            reply(False, "gflow не найден, проверьте gflow_path в config.ini")
        except OSError as e:
            reply(False, "не удалось запустить gflow: %s" % e)
        files = merge_files(list_new_files(out_dir, before),
                            paths_from_text(out + "\n" + err))
        if code == 0 and files:
            break
        if timed_out or code in EXIT_HINTS:
            break  # retrying will not help
        if attempt + 1 < attempts:
            time.sleep(3)

    note = ""
    if ignored:
        note = " (пропущены неподдерживаемые параметры: %s)" % ", ".join(ignored)

    if code == 0 and files:
        reply(True, "Готово, файл: " + "; ".join(str(p) for p in files[-4:]) + note)

    # Generation may have succeeded while the download failed: fetch the clip
    # by id instead of generating (and paying for) it again.
    clip = CLIP_ID_RE.search(ANSI_RE.sub("", out + "\n" + err))
    if clip and not timed_out:
        try:
            c2, o2, e2, t2 = run_cmd([gflow, "data", "download", clip.group(1)],
                                     env, min(timeout, 240))
            files = merge_files(list_new_files(out_dir, before),
                                paths_from_text(o2 + "\n" + e2))
            if files:
                reply(True, "Готово (скачано повторно), файл: "
                      + "; ".join(str(p) for p in files[-4:]) + note)
        except OSError:
            pass
        reply(False, "Ролик создан, но не скачался. Скачайте вручную: "
                     "gflow data download %s. Генерировать заново не нужно."
                     % clip.group(1))

    if timed_out:
        msg = ("gflow не уложился во время (%d с). Ролик мог дойти до Flow, "
               "проверьте проект во Flow." % timeout)
        reply(False, msg + (VIDEO_WARN if is_video else ""))

    if code == 0:
        msg = ("gflow завершился успешно, но файл не найден в %s. %s"
               % (out_dir, clean_tail(err or out)))
    else:
        hint = EXIT_HINTS.get(code)
        msg = "gflow завершился с кодом %s: %s" % (code, clean_tail(err or out))
        if hint:
            msg += " Подсказка: " + hint + "."
    reply(False, msg + (VIDEO_WARN if is_video else ""))


def main():
    try:
        # Qt writes UTF-8; do not rely on the locale encoding of sys.stdin.
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