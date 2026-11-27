#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <grpcpp/grpcpp.h>
#include "greeter.grpc.pb.h"

class GreeterService final : public homework::Greeter::Service {
public:
    grpc::Status SayHello(grpc::ServerContext*,
                          const homework::HelloRequest* request,
                          homework::HelloReply* reply) override {
        const std::string name = request->name().empty() ? "Гость" : request->name();
        reply->set_message("Привет, " + name + "! Это Unary-ответ сервера.");
        return grpc::Status::OK;
    }

    grpc::Status WatchGreetings(
        grpc::ServerContext* context,
        const homework::HelloRequest* request,
        grpc::ServerWriter<homework::HelloReply>* writer) override {
        const std::string name = request->name().empty() ? "Гость" : request->name();

        for (int i = 1; i <= 5; ++i) {
            if (context->IsCancelled()) {
                return grpc::Status(grpc::StatusCode::CANCELLED, "Клиент отключился");
            }
            homework::HelloReply reply;
            reply.set_message("Сообщение " + std::to_string(i) + " для " + name);
            if (!writer->Write(reply)) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(400));
        }
        return grpc::Status::OK;
    }
};

int main(int argc, char** argv) {
    const std::string address = argc > 1 ? argv[1] : "0.0.0.0:50051";
    GreeterService service;
    grpc::ServerBuilder builder;
    builder.AddListeningPort(address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    if (!server) {
        std::cerr << "Не удалось запустить сервер: " << address << '\n';
        return 1;
    }
    std::cout << "gRPC-сервер запущен: " << address << '\n';
    server->Wait();
    return 0;
}
