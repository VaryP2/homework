Сообщения

1 Сообщение 2 Поле 3 Тип 4 Номер 

1 HelloRequest 2 name 3 string 4 1 
1 HelloReply 2 message 3 string 4 1 

 Сервис Greeter

1 Метод 2 Тип RPC 3 Запрос 4 Ответ 

1 SayHello 2 Unary 3 HelloRequest 4 HelloReply 
1 WatchGreetings 2 Server streaming 3 HelloRequest 4 поток HelloReply 

Unary — один запрос и один ответ. Server streaming — один запрос и несколько ответов в одном потоке.


