{
  "plugin_name": "gflow",
  "exec": "python",
  "args": ["gflow_plugin.py"],
  "timeout_ms": 3600000,
  "version": "1.3.0",
  "description": "Плагин для генерации картинок и видео через Google Flow (неофициальный gflow-cli)",
  "functions": [
    {
      "name": "generateImage",
      "description": "Генерирует картинку по текстовому промпту (aspect, model, count) и сохраняет файл",
      "handler_type": "gflow_image"
    },
    {
      "name": "generateVideo",
      "description": "Генерирует видео по текстовому промпту (aspect, resolution, duration, model, filename), тратит кредиты, занимает несколько минут",
      "handler_type": "gflow_video"
    }
  ]
}