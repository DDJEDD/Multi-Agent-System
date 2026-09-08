<div align="center">

# 🧠 Agent Console

### A self-organizing, Gemini-powered multi-agent orchestrator with a Telegram bot and a native Qt desktop client

![C++](https://img.shields.io/badge/C%2B%2B-17-00599C?style=flat-square&logo=cplusplus)
![Qt](https://img.shields.io/badge/Qt-6.10-41CD52?style=flat-square&logo=qt)
![Gemini](https://img.shields.io/badge/LLM-Gemini%203.6%20Flash-8E75B2?style=flat-square)
![Platform](https://img.shields.io/badge/Platform-Windows%20%7C%20Linux-lightgrey?style=flat-square)
![License](https://img.shields.io/badge/License-Unlicensed-lightgrey?style=flat-square)

</div>

---

A single **Orchestrator** agent (Главный агент / *"Main Agent"*) talks to the user, and can dynamically **create, rename, edit, and delete** its own specialized **sub‑agents** — each with its own persona and prompt files — delegating tasks to them at will. Every decision the system makes is driven by a single Gemini call per turn, which returns a strict JSON **function‑call stack** that the app parses and executes.

The same backend powers two very different front ends:

| | |
|---|---|
| 💬 **TgBot** | A Telegram bot — long-polls `getUpdates`, sends text, photos, and stickers back to the chat. |
| 🖥️ **Agent Console** | A native Qt desktop chat client — sidebar agent roster, live status, themeable UI, and a typewriter-style code panel. |

---

## Table of Contents

- [How it works](#-how-it-works)
- [Architecture](#-architecture)
- [Agents on disk](#-agents-on-disk)
- [Roles & permissions](#-roles--permissions)
- [Function-calling protocol](#-function-calling-protocol)
- [JSON response sanitization](#-json-response-sanitization)
- [Reliability: retries & number masking](#-reliability-retries--number-masking)
- [Desktop UI tour](#-desktop-ui-tour)
- [Theming](#-theming)
- [Requirements](#-requirements)
- [Setup & Build](#-setup--build)
- [Usage](#-usage)
- [Project layout](#-project-layout)

---

## 🔄 How it works

```
User message  ──▶  agents::reqAgent()
                       │
                       ├─ builds full context:
                       │    soul + min_info + system_prompt + stickers
                       │    + conversation history
                       │    + list of available sub-agents
                       │
                       ▼
                 Gemini generateContent
                       │
                       ▼
        ┌───────────────────────────────┐
        │  strict JSON response:         │
        │  { "function_call_stack": [...] }
        └───────────────┬───────────────┘
                         ▼
                   JSONParser::parse
              (sanitize → extract → validate)
                         ▼
                 QList<AgentCall>
                         ▼
               agents::executeCall()  ──▶  for each call:
                                             • send messages/stickers/photos
                                             • createAgent / deleteAgent
                                             • changeAgentName
                                             • editFile / getFile
                                             • reqAgent → delegate to sub-agent
```

1. Incoming text (from Telegram **or** the desktop UI) is forwarded to `agents::reqAgent()`.
2. A single prompt is assembled from the target agent's `soul`, `min_info`, `system_prompt`, and `stickers` files, plus recent conversation history and a system note listing every currently available sub-agent.
3. The request is sent to `gemini-3.6-flash` with `responseMimeType: application/json`, so the model is constrained to return JSON.
4. `JSONParser` cleans up the raw text and parses it into an ordered list of `AgentCall` structs.
5. `agents::executeCall()` walks the list and dispatches each call — enforcing role permissions along the way.
6. Replies (text, delayed multi-part messages, stickers, photos, code blocks) are routed back to whichever front end started the request, tagged by a `MessageSource` (`Telegram` or `UI`) that flows through the entire call chain — including nested `reqAgent` delegations.
7. The desktop UI additionally receives fine-grained `requestThinkingContext` events at every step of execution (function dispatched, agent created, file edited, …), rendered as a pulsing **"🧠 thinking…"** chip above the composer.

---

## 🏗 Architecture

```
                         ┌───────────────────┐
                         │       agents        │   agent lifecycle,
                         │  (agents.h/.cpp)     │   prompt assembly,
                         └─────────┬───────────┘   function dispatch
             ┌─────────────────────┼─────────────────────┐
             │                     │                     │
     ┌───────▼────────┐   ┌────────▼────────┐   ┌────────▼─────────┐
     │    JSONParser    │   │   FileManager    │   │   phonenumber     │
     │  sanitize/parse   │   │  agent files &    │   │  mask/restore      │
     │  Gemini's reply    │   │  chat history      │   │  phone numbers      │
     └────────────────┘   └─────────────────┘   └───────────────────┘
             ▲
             │  MessageSource::UI / MessageSource::Telegram
             │
    ┌────────┴─────────┐              ┌────────────────────┐
    │      TgBot          │              │     MainWindow       │
    │  (Telegram polling,  │              │  (Qt desktop chat,    │
    │  send msg/photo/sticker)│           │  agent roster, themes, │
    └──────────────────┘              │  live code panel)     │
                                       └────────────────────┘
```

- **`agents`** — the brain: builds Gemini requests, retries on transient failure, parses results, and enforces the permission model when dispatching function calls.
- **`JSONParser`** — a defensive parser built to survive an LLM's occasional sloppiness (see [JSON sanitization](#-json-response-sanitization) below).
- **`FileManager`** — thin wrapper around agent folders and per-chat history files on disk.
- **`phonenumber`** — strips phone numbers out of user text before it ever reaches the LLM, and re-inserts them into outgoing messages.
- **`MainWindow`** — the desktop client: everything from chat bubbles to the theme editor lives here.

---

## 📁 Agents on disk

Every agent — Orchestrator included — is just a folder:

```
<app-dir>/Agents/<agentName>/
├── system_prompt.md   # behavioral rules / instructions
├── soul.md             # personality / voice
├── min_info.md          # display name + short role note (parsed by the UI)
└── stickers.md           # optional: sticker ID reference sheet
```

| Function | What it does |
|---|---|
| `createAgent(name, purpose)` | Creates the folder and seeds `system_prompt`, `soul`, and `min_info`. |
| `getFullPrompt(name)` | Concatenates `soul + min_info + system_prompt + stickers` into the LLM context, skipping any empty files. |
| `editFile` / `getFile` | Lets the Orchestrator (or the desktop UI's settings dialog) read/write any prompt file at runtime. |
| `changeAgentName` | Renames the agent's folder on disk (refuses if the target name already exists). |
| `deleteAgent` | Removes the agent's folder entirely. |
| `listAgents()` | Enumerates every subfolder of `Agents/` — this is how both the LLM and the UI discover which sub-agents currently exist. |

`min_info` stores the agent's display name on its first line (`Имя агента: …`) with the free-text role note below it — the UI parses and rebuilds this file (`extractRoleFromMinInfo` / `buildMinInfo`) so the name and role can be edited independently.

---

## 🔐 Roles & permissions

| Role | Send messages/photos/stickers to the user | Manage agents (create/delete/rename/edit files) | Call `reqAgent` |
|---|:---:|:---:|:---:|
| **Orchestrator** | ✅ | ✅ | ✅ — to *any* sub-agent |
| **Sub-agent** | ❌ | ❌ | ✅ — *only* back to the Orchestrator |

This is enforced in code, not just prompted for: `executeCall()` checks `role == Constants::RoleOrchestrator` before honoring privileged calls, and a sub-agent attempting to call another sub-agent is rejected with a logged security warning.

---

## 🧩 Function-calling protocol

The model is instructed to respond with **only** a JSON object shaped like this:

```json
{
  "function_call_stack": [
    {
      "id": "call-1",
      "role": "orchestrator",
      "agentName": "Главный агент",
      "name": "messages",
      "args": {
        "messages": [
          { "text": "Hello! 👋", "delay": 0 },
          { "text": "One moment…", "delay": 1200, "code": "print('hi')" }
        ]
      }
    }
  ]
}
```

Two envelope shapes are accepted, so the parser is tolerant of minor prompt-format drift:

- a bare `{ "function_call_stack": [...] }` object,
- or `{ "agent_controls_context": { "function_call_stack": [...] } }`,
- or a raw top-level array `[...]`.

| Function | Purpose | Restricted to |
|---|---|---|
| `messages` | Send one or more delayed messages, each with optional `stickerId`, `photoUrl`, and `code` (rendered as a collapsible code block / streamed into the code panel). | any role |
| `createAgent` | Create a new sub-agent with a given purpose. | Orchestrator |
| `deleteAgent` | Delete an existing agent. | Orchestrator |
| `changeAgentName` | Rename an agent. | Orchestrator |
| `editFile` | Overwrite one of an agent's prompt files. | Orchestrator |
| `getFile` | Read one of an agent's prompt files. | Orchestrator |
| `reqAgent` | Delegate a task to another agent (with an optional base64 photo payload). | any role, but scoped as per the permission table above |

Each `messages` entry's `delay` is clamped to `0–5000` ms before being scheduled, so a malformed or hallucinated value can never stall the UI.

---

## 🧼 JSON response sanitization

LLMs don't always produce perfectly clean JSON, so `JSONParser::sanitizeJsonText()` runs a small pipeline before parsing:

1. **Strip Markdown code fences** — a leading ` ```json ` / ` ``` ` and trailing ` ``` ` are removed if present.
2. **Trim trailing garbage** — anything after the last `}` is discarded.
3. **Drop `thoughtSignature` fields** — an internal Gemini artifact that would otherwise break parsing.
4. **Fallback brace-matching** — if `QJsonDocument::fromJson` still fails (e.g. `GarbageAtEnd`), `extractFirstJsonObject()` walks the text character-by-character (respecting string/escape boundaries) to find the first balanced `{ … }` block and re-parses just that.

Only after all of this does the parser walk the resulting `function_call_stack`, normalizing several historically-inconsistent key names along the way (`functionName`/`name`, `agentName`/`agent_name`, `agent_name` nested inside `args`, etc.) — so a slightly-off model response still produces a usable `AgentCall`.

---

## 🔁 Reliability: retries & number masking

- **Automatic retry with backoff** — Gemini errors `429`, `500`, and `503` are retried up to `kMaxRetries` times, with delay `kRetryBaseDelayMs * 2^retryCount` between attempts. Non-retryable errors (or exhausted retries) surface as a `requestError` signal instead of failing silently.
- **Phone number masking** — before any user text reaches Gemini, `phonenumber::HideNumbers()` replaces phone numbers with placeholders; the returned map of placeholders → real numbers (`nums`) is threaded through the whole response pipeline so `JSONParser::parse()` can restore the real numbers into outgoing message text via `phone.restoreNumbers()`.

---

## 🖥️ Desktop UI tour

<div align="center">

**Sidebar** · **Chat** · **Live Code Panel** · **Settings**

</div>

- **🧑‍🤝‍🧑 Agent roster (sidebar)**
  - A pinned status card for the Orchestrator: avatar, live online/offline dot, editable name, role preview, and app uptime.
  - Below it, one card per sub-agent: generated initials-avatar, colored status dot (`Idle` / `Active` / `Busy` / `Error`), inline-editable name, enable/disable toggle switch, and a gear button for full settings.
  - `+` creates a brand-new sub-agent on the spot; `⟳` re-scans `Agents/` on disk to pick up anything the Orchestrator created or removed itself; `◂` collapses the whole sidebar down to icons with a smooth width animation.

- **💬 Chat area**
  - Per-agent conversation history, rendered as gradient-accented bubbles for the user and neutral bubbles for the agent, each with a fade-in entrance animation.
  - Long code snippets collapse into a `▸ Код` header you can expand/collapse and copy with one click.
  - A pulsing **"🧠 thinking…"** chip appears above the composer while a request is in flight, showing the latest live progress message, and auto-fades out a few seconds after the agent goes quiet.

- **⚡ Live code panel**
  - A slide-out panel on the right that "types" incoming code character-by-character in small randomized chunks, with a blinking text cursor — purely cosmetic, but makes long generations feel alive rather than dumped all at once.
  - One click copies the full snippet to the clipboard; another closes the panel with a matching slide animation.

- **⚙️ Agent settings dialog**
  - Edit an agent's display name and short role note.
  - Tabbed editors for its `soul` and `system_prompt` files.
  - Sub-agents get a destructive **"Delete agent"** button with a confirmation prompt; the Orchestrator does not.

---

## 🎨 Theming

Everything is driven by a small palette system (`ThemePalette`) computed from a handful of *key* colors — background, panel, card, border, text, two accents, success, and danger — with hover states, muted text tones, and status colors derived automatically based on whether the base color is light or dark.

**Built-in theme families**, each with a light and dark variant:

| Family | Light | Dark |
|---|---|---|
| Neutral | Daylight | Midnight |
| Pink | Sakura | Yozakura |
| Nord | Nord Light | Nord |
| Sunset | Sunrise | Sunset |

Every family can be fine-tuned or forked into a fully **custom theme** via an in-app color picker (`openThemeCreator`), which writes overrides straight into `config.ini`.

**Backdrop styles** — chosen independently of the color theme, and live-animated at ~7.7 FPS:

| Mode | Description |
|---|---|
| `gradient` | Softly drifting radial color blobs |
| `aurora` | A slow-rotating diagonal aurora-style gradient sweep |
| `particles` | Twinkling ambient dots along the window edges |
| `starfall` | Streaking meteor trails with fading tails |
| *custom image/GIF* | Your own picture or animated GIF, cover-fit and centered |
| `none` | Flat background, animation disabled |

Switching **any** theme or backdrop setting triggers a full, near-instant live UI rebuild (`rebuildUiLive`) that preserves your current selection, scroll position, and open settings page — no restart required.

---

## 📋 Requirements

- **Qt 6** (developed against Qt 6.10, MinGW 64‑bit)
- A **C++17**-compatible compiler
- A **Telegram Bot token** (from [@BotFather](https://t.me/BotFather)) — only needed for the Telegram front end
- A **Google Gemini API key**

---

## 🚀 Setup & Build

1. **Clone** the repository and open it in Qt Creator, or configure it directly via CMake/qmake.
2. **Provide credentials** — either through `config.ini` (also editable live from the desktop app's Settings page) or environment variables loaded at startup:
   ```ini
   TELEGRAM_BOT_TOKEN=your_telegram_token
   GEMINI_API_KEY=your_gemini_api_key
   ```
3. **Build**:
   ```bash
   qmake MCP_Agent.pro && make
   # — or, with CMake —
   cmake -B build -S .
   cmake --build build
   ```
4. **Run** the resulting executable. On first launch it will:
   - create the `Agents/` folder next to the binary,
   - seed the default Orchestrator agent (**Главный агент**),
   - and generate a default `config.ini` (dark theme, animated gradient backdrop).

---

## 💡 Usage

**Telegram**
- Send any message — it's routed straight to the Orchestrator.
- `/generate <description>` (alias `/image`) asks Gemini's image model to generate an image and sends it back.
- `/start` shows the bot's intro message.

**Desktop app**
- Pick the Orchestrator or any sub-agent from the sidebar and start typing — `Enter` sends, `Shift+Enter` inserts a newline.
- `+` in the sidebar header spins up a brand-new sub-agent instantly.
- The gear icon on any card opens its full settings (name, role, `soul`, `system_prompt`).
- The gear icon at the bottom of the sidebar opens the app-wide Settings page (theme, backdrop, API keys).

**Either front end** — talk to the Orchestrator in plain language and it will translate your intent into the right function calls:

> *"Create a sub-agent called 'Support' whose job is to answer FAQs about our product."*
> *"Rename 'Support' to 'Помощник'."*
> *"Ask the Помощник agent what our refund policy is."*
> *"Delete the Помощник agent, we don't need it anymore."*

---

## 🗂 Project layout

| File | Responsibility |
|---|---|
| `tgbot.h/.cpp` | Telegram long-polling, sending messages/photos/stickers, wiring Gemini requests |
| `agents.h/.cpp` | Agent lifecycle (create/delete/rename), prompt assembly, function-call execution, retry logic |
| `jsonparser.h/.cpp` | Sanitizing and parsing Gemini's JSON `function_call_stack` responses |
| `filemanager.h/.cpp` | Low-level file/directory operations backing each agent's prompt files and chat history |
| `mainwindow.h/.cpp` | Desktop chat UI: sidebar, chat bubbles, live code panel, theming, settings dialogs |

---

<div align="center">

*Built with Qt, C++17, and a lot of JSON parsing defensiveness.*

</div>
