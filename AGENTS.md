# Инструкции для агентов проекта sdr-app

Рабочая папка проекта: `D:/Projects/sdr-app`. Выполняй команды и сохраняй изменения проекта в этой папке.

## Процесс разработки

- Веди разработку всех этапов проекта по OpenSpec.
- Перед реализацией создавай или актуализируй соответствующее изменение OpenSpec: требования, проектные решения и задачи. Выполняй реализацию по этим задачам, проверяй соответствие спецификациям и архивируй завершённое изменение по процессу OpenSpec.
- Артефакты OpenSpec размещай в структуре OpenSpec, а архитектурные решения фиксируй в `docs/sdr/` со ссылками из соответствующих изменений OpenSpec.

## Git

- Перед подготовкой изменений, коммитов, PR, ревью и релизов читай и применяй [CONTRIBUTING.md](CONTRIBUTING.md). Правила этого файла уточняют общие соглашения CONTRIBUTING.md.
- Основная рабочая ветка — `main`. Создавай коммиты в `main` по завершении логически связанного изменения или проверенного этапа работы без повторного запроса разрешения.
- Перед коммитом проверяй состав изменений и выполняй соответствующие им проверки. Сообщение коммита должно описывать сохранённый результат; незавершённые задачи и открытые решения сохраняют свой статус.

Do not include unrelated formatting changes, temporary output, local configuration, caches, build artifacts, or secrets.

When creating commits:

- use Conventional Commits;
- preserve the human developer as the primary author;
- add AI attribution using `Co-authored-by` trailers at the end of the commit message;
- for Claude Code contributions, add:
  `Co-authored-by: Claude <noreply@anthropic.com>`
- for ChatGPT/Codex contributions, add:
  `Co-authored-by: Codex <codex@openai.com>`
- when both Claude Code and ChatGPT/Codex contributed to the same commit, add both trailers;
- separate the trailers from the commit message body with a blank line.

## Архитектурные решения

- Сохраняй все архитектурные решения SDR-проекта в `docs/sdr/` относительно корня проекта. Это включает ADR, архитектурные спецификации, API-контракты и реестр открытых решений.
- Перед архитектурными изменениями изучай относящиеся к задаче материалы в `docs/sdr/`; после согласования решения обновляй соответствующие документы там же.
- Явно различай принятые решения, предложения и открытые вопросы. Открытые решения сохраняют статус «не согласовано» до явного подтверждения пользователя.
