# MAS

A Qt/C++ Telegram bot that acts as an **LLM-driven multi-agent orchestrator**. A single "Orchestrator" agent (Главный агент) talks to the user on Telegram, and can dynamically create, edit, rename, and delete specialized "sub-agents" — each with its own prompt files — and delegate tasks to them. All reasoning is powered by Google's Gemini API, with structured JSON function-calling used to control the bot's behavior.

## How it works

1. `TgBot` long-polls the Telegram Bot API (`getUpdates`) for new messages and photos.
2. Incoming text is forwarded to `TgBot::reqAgent()`, which builds a full prompt (agent's `soul` + `min_info` + `system_prompt` + `stickers`, conversation history, and available sub-agent list) and sends it to Gemini (`generateContent`).
3. Gemini is instructed to reply with a strict JSON object containing a `function_call_stack` — an ordered list of function calls the model wants to execute (send messages, create/delete/rename an agent, edit or read an agent's file, or delegate to a sub-agent via `reqAgent`).
4. `JSONParser` sanitizes and parses that JSON (stripping code fences, `thoughtSignature` blobs, and recovering from trailing garbage) into a list of `AgentCall` structs.
5. `agents::executeCall()` dispatches each call: only the Orchestrator role may send messages/photos/stickers to the user or manage agents; sub-agents may only call back into the Orchestrator via `reqAgent`.
6. Delayed/multi-part replies (text, stickers, photos) are sent back to Telegram with per-message delays via `TgBot::sendMessagesDelayed()`.

## Architecture

```
TgBot (Telegram polling, sending messages/photos/stickers)
  │
  ├── phonenumber        — masks/restores phone numbers before/after sending text to the LLM
  ├── JSONParser          — sanitizes & parses Gemini's JSON function_call_stack response
  └── agents              — manages sub-agent lifecycle & executes parsed function calls
        │
        └── FileManager   — reads/writes each agent's prompt files on disk
```

### Agents

Each agent (Orchestrator or sub-agent) is represented on disk as a folder:

```
<app-dir>/Agents/<agentName>/
  system_prompt.md
  soul.md
  min_info.md
  stickers.md   (optional)
```

- **`createAgent(name, purpose)`** creates the folder and seeds `system_prompt`, `soul`, and `min_info` files.
- **`getFullPrompt(name)`** concatenates `soul` + `min_info` + `system_prompt` + `stickers` into the context sent to Gemini.
- **`editFile` / `getFile`** let the Orchestrator update or inspect an agent's prompt files at runtime.
- **`changeAgentName` / `deleteAgent`** rename or remove an agent's folder.

### Roles & permissions

| Role           | Can send messages/photos/stickers | Can manage agents | Can call `reqAgent` |
|----------------|:---:|:---:|:---:|
| Orchestrator   | ✅ | ✅ | ✅ (to any sub-agent) |
| Sub-agent      | ❌ | ❌ | ✅ (only back to the Orchestrator) |

### Function-calling protocol

The model must respond with **only** a JSON object shaped like:

```json
{
  "function_call_stack": [
    {
      "role": "orchestrator",
      "agentName": "Главный агент",
      "name": "messages",
      "args": {
        "messages": [
          { "text": "Hello!", "delay": 0 }
        ]
      }
    }
  ]
}
```

Supported functions: `messages`, `createAgent`, `deleteAgent`, `changeAgentName`, `editFile`, `getFile`, `reqAgent`.

## Requirements

- Qt 6 (developed against Qt 6.10, MinGW 64-bit)
- A C++17-compatible compiler
- A Telegram Bot token ([@BotFather](https://t.me/BotFather))
- A Google Gemini API key

## Setup & Build

1. Clone the repository and open the project in Qt Creator (or configure via CMake/qmake directly).
2. Provide your credentials — e.g. via a `.env` file or environment variables loaded at startup:
   ```
   TELEGRAM_BOT_TOKEN=your_telegram_token
   GEMINI_API_KEY=your_gemini_api_key
   ```
   (Wire these into wherever `token` and `geminiKey` are currently assigned in the bot's setup code.)
3. Build the project:
   ```bash
   qmake MCP_Agent.pro && make
   # or, with CMake:
   cmake -B build -S .
   cmake --build build
   ```
4. Run the resulting executable. On first run it will create the `Agents/` directory next to the binary and seed the default Orchestrator agent (`Главный агент`).

## Usage

- Send any message to the bot — it's routed to the Orchestrator.
- `/generate <description>` (or `/image`) asks Gemini's image model to generate an image and sends it back.
- `/start` shows the bot's intro message.
- Ask the Orchestrator (in natural language) to create, rename, or delete sub-agents, edit their prompts, or delegate a task to them — it will translate this into the appropriate function calls.

## Project layout

| File | Responsibility |
|---|---|
| `tgbot.h/.cpp` | Telegram long-polling, sending messages/photos/stickers, wiring Gemini requests |
| `agents.h/.cpp` | Agent lifecycle (create/delete/rename), prompt assembly, function-call execution |
| `jsonparser.h/.cpp` | Sanitizing and parsing Gemini's JSON `function_call_stack` responses |
| `filemanager.h/.cpp` | Low-level file/directory operations backing each agent's prompt files |
