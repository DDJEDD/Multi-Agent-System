{
  "plugin_name": "ffmpeg",
  "exec": "python",
  "args": ["ffmpeg_plugin.py"],
  "timeout_ms": 3600000,
  "version": "1.0.0",
  "description": "Плагин для сборки готового ролика из клипов, озвучки и музыки: переходы, субтитры, надписи, громкость (ffmpeg)",
  "functions": [
    {
      "name": "renderVideo",
      "description": "Собирает видео по JSON-проекту (clips, voice, music, subtitles, accents, transition) и сохраняет mp4",
      "handler_type": "ffmpeg_render"
    }
  ]
}