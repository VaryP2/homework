Что реализовано

- отдельный pthread для каждого TCP-подключения;
- общий счётчик активных клиентов;
- доступ к счётчику и списку клиентов защищён pthread_mutex_t;
- обработка SIGINT/SIGTERM;
- корректное закрытие listening socket;
- shutdown() всех активных клиентских сокетов при завершении;
- pthread_join() для ожидания всех созданных рабочих потоков;
- закрытие каждого клиентского файлового дескриптора;
- SIGPIPE отключён, чтобы разрыв соединения не завершал процесс;
- сервер работает как простой echo-сервер.

Сборка
gcc -Wall -Wextra -Wpedantic -O2 -pthread src/server.c -o server

Запуск

./server

Сервер слушает:

0.0.0.0:8080

Остановить:

Ctrl+C


 Проверка тремя клиентами

nc 127.0.0.1 8080

В каждый клиент отправьте своё сообщение, например:

client-1
client-2
client-3

Сервер должен показывать изменения общего счётчика:

[server] listening on 0.0.0.0:8080
[server] clients=1
[worker] connected: 127.0.0.1:...
[server] clients=2
[worker] connected: 127.0.0.1:...
[server] clients=3
[worker] connected: 127.0.0.1:...
[server] client disconnected, clients=2
[server] client disconnected, clients=1
[server] client disconnected, clients=0


Файл .txt содержит пример ожидаемого вывода.
