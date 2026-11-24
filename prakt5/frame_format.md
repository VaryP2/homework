 Разбор WebSocket-кадра

В программе frame_parser.c разбираются:

- FIN — последний ли это фрагмент;
- opcode — тип кадра;
- MASK — наличие masking key;
- длина payload;
- masking key;
- payload после XOR-декодирования.

Основные значения opcode:

0x0 — continuation
0x1 — text
0x2 — binary
0x8 — close
0x9 — ping
0xA — pong

