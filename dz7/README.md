# Сервер с синхронизацией и эссе

Учебный проект по домашнему заданию: **thread-per-connection**, общий счётчик клиентов под `pthread_mutex`, корректное закрытие сокетов и завершение сервера.

## Что реализовано

- отдельный `pthread` для каждого TCP-подключения;
- общий счётчик активных клиентов;
- доступ к счётчику и списку клиентов защищён `pthread_mutex_t`;
- обработка `SIGINT`/`SIGTERM`;
- корректное закрытие listening socket;
- `shutdown()` всех активных клиентских сокетов при завершении;
- `pthread_join()` для ожидания всех созданных рабочих потоков;
- закрытие каждого клиентского файлового дескриптора;
- `SIGPIPE` отключён, чтобы разрыв соединения не завершал процесс;
- сервер работает как простой echo-сервер.

## Сборка

```bash
gcc -Wall -Wextra -Wpedantic -O2 -pthread src/server.c -o server
```

Или:

```bash
make
```

## Запуск

```bash
./server
```

Сервер слушает:

```text
0.0.0.0:8080
```

Остановить:

```text
Ctrl+C
```

## Проверка тремя клиентами

Откройте три терминала и выполните:

```bash
nc 127.0.0.1 8080
```

В каждый клиент отправьте своё сообщение, например:

```text
client-1
client-2
client-3
```

Сервер должен показывать изменения общего счётчика:

```text
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
```

Файл `output/three_clients.txt` содержит пример ожидаемого вывода.

## Проверка санитайзерами

Для поиска ошибок памяти можно собрать:

```bash
gcc -Wall -Wextra -g -fsanitize=address,undefined \
    -pthread src/server.c -o server_asan
```

Для проверки гонок в Linux удобно использовать ThreadSanitizer:

```bash
gcc -Wall -Wextra -g -fsanitize=thread \
    -pthread src/server.c -o server_tsan
```

Затем запустить сервер и подключить несколько клиентов.

## Структура

```text
sync_server_essay/
├── src/
│   └── server.c
├── docs/
│   └── essay.md
├── output/
│   └── three_clients.txt
├── Makefile
└── README.md
```
