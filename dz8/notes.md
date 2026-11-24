Структура запроса

1. Стартовая строка: метод, URL и версия протокола.
2. Заголовки: например, Host, Content-Type, Content-Length.
3. Пустая строка.
4. Тело запроса (если оно есть).

Используемые статусы

- 200 OK — запрос успешно выполнен.
- 201 Created — ресурс создан.
- 400 Bad Request — запрос некорректен.
- 404 Not Found — ресурс или маршрут не найден.

Content-Type статических файлов

Сервер выбирает тип содержимого по расширению:
- .html - text/html; charset=utf-8
- .css - text/css; charset=utf-8
- .js - application/javascript; charset=utf-8
- .json - application/json; charset=utf-8
- прочие файлы - application/octet-stream
