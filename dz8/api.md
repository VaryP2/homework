 Схема вызовов API


 Операция  Метод  URL  Тело запроса  Успешный статус 

 Получить список  GET  /api/items  нет  200 OK 
 Создать запись  POST  /api/items  {"title":"Новая запись"}  201 Created 
 Изменить запись №1  PUT  /api/items/1  {"title":"Новое название"}  200 OK 
 Удалить запись №1  DELETE  /api/items/1  нет  200 OK 


Получение списка

GET /api/items HTTP/1.1
Host: 127.0.0.1:8080
Connection: close


Ответ: 200 OK, тело — JSON-массив записей.

 Создание

POST /api/items HTTP/1.1
Host: 127.0.0.1:8080
Content-Type: application/json
Content-Length: 24
Connection: close

{"title":"Новая запись"}


Ответ:201 Created, тело содержит созданную запись с назначенным id.

Изменение

PUT /api/items/1 HTTP/1.1
Host: 127.0.0.1:8080
Content-Type: application/json
Connection: close

{"title":"Новое название"}


Ответ: 200 OK. Если записи нет — 404 Not Found.

 Удаление

DELETE /api/items/1 HTTP/1.1
Host: 127.0.0.1:8080
Connection: close


Ответ: 200 OK. Если записи нет — 404 Not Found.

Коды ошибок

 Код  Причина 

 400 Bad Request  Некорректная строка запроса, неверный ID или отсутствует поле title 
 404 Not Found  Неизвестный URL, отсутствующий статический файл или запись с таким ID 
