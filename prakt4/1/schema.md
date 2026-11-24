Схема вызовов /users

 1 Операция  2 HTTP-запрос 3 Назначение 4 Ожидаемый статус 
1 Create 2POST /users 3 Создать пользователя 4 201 Created
1 Read 2GET /users 3 Получить список пользователей 4 200 OK
1 Read one 2GET /users/{id} 3 Получить пользователя 4 200 OK
1 Update 2PUT /users/{id} 3 Полностью обновить пользователя 4 200 OK 
1 Partial update 2PATCH /users/{id} 3 Частично обновить пользователя 4 200 OK 
1 Delete 2DELETE /users/{id} 3 Удалить пользователя 4 204 No Content
1 Not found 2GET /users/{id} 3 Несуществующий пользователь 4 404 Not Found 

