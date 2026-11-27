Проверка проекта

Сначала сервер:

./build/grpc_server

В другом терминале Unary:

./build/unary_client localhost:50051 Алексей

Server streaming:

./build/streaming_client localhost:50051 Мария

Асинхронный клиент:

./build/async_client localhost:50051

