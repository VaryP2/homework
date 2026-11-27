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
    homework::HelloReply reply;
    grpc::ClientContext context;

    grpc::Status status = stub->SayHello(&context, request, &reply);
    if (!status.ok()) {
        std::cerr << "Ошибка gRPC: " << status.error_message() << '\n';
        return 1;
    }
    std::cout << "Ответ сервера: " << reply.message() << '\n';
    return 0;
}
