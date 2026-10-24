{
  "plugin_name": "inworld",
  "exec": "inworldportal.exe",
  "timeout_ms": 60000,
  "version": "1.0.0",
  "description": "Плагин для синтеза речи (TTS) через Inworld Portal API",
  "functions": [
    {
      "name": "ReqTTSAudio",
      "description": "Генерирует озвучку из текста и сохраняет MP3 файл",
      "handler_type": "inworld_tts"
    }
  ]
}