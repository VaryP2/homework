#include <iostream>
#include <memory>
#include <string>
#include <grpcpp/grpcpp.h>
#include "greeter.grpc.pb.h"

int main(int argc, char** argv) {
    const std::string target = argc > 1 ? argv[1] : "localhost:50051";
    const std::string name = argc > 2 ? argv[2] : "Студент";
    auto channel = grpc::CreateChannel(target, grpc::InsecureChannelCredentials());
    auto stub = homework::Greeter::NewStub(channel);

    homework::HelloRequest request;
    request.set_name(name);
    grpc::ClientContext context;
    auto reader = stub->WatchGreetings(&context, request);

    homework::HelloReply reply;
    std::cout << "Сообщения от сервера:\n";
    while (reader->Read(&reply)) {
        std::cout << "  " << reply.message() << '\n';
    }

    grpc::Status status = reader->Finish();
    if (!status.ok()) {
        std::cerr << "Ошибка потока: " << status.error_message() << '\n';
        return 1;
    }
    std::cout << "Поток завершён.\n";
    return 0;
}
