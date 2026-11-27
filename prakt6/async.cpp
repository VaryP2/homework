#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <grpcpp/grpcpp.h>
#include "greeter.grpc.pb.h"

using Clock = std::chrono::steady_clock;

struct AsyncCall {
    homework::HelloReply reply;
    grpc::ClientContext context;
    grpc::Status status;
    std::unique_ptr<grpc::ClientAsyncResponseReader<homework::HelloReply>> response;
    int number = 0;
};

int main(int argc, char** argv) {
    const std::string target = argc > 1 ? argv[1] : "localhost:50051";
    const int count = 5;
    auto channel = grpc::CreateChannel(target, grpc::InsecureChannelCredentials());
    auto stub = homework::Greeter::NewStub(channel);

    auto sync_start = Clock::now();
    for (int i = 1; i <= count; ++i) {
        homework::HelloRequest request;
        request.set_name("Sync " + std::to_string(i));
        homework::HelloReply reply;
        grpc::ClientContext context;
        grpc::Status status = stub->SayHello(&context, request, &reply);
        if (!status.ok()) {
            std::cerr << "Sync ошибка: " << status.error_message() << '\n';
            return 1;
        }
    }
    auto sync_end = Clock::now();

    grpc::CompletionQueue queue;
    std::vector<std::unique_ptr<AsyncCall>> calls;
    calls.reserve(count);
    auto async_start = Clock::now();

    for (int i = 1; i <= count; ++i) {
        auto call = std::make_unique<AsyncCall>();
        call->number = i;
        homework::HelloRequest request;
        request.set_name("Async " + std::to_string(i));
        call->response = stub->AsyncSayHello(&call->context, request, &queue);
        call->response->Finish(&call->reply, &call->status, call.get());
        calls.push_back(std::move(call));
    }

    int completed = 0;
    void* tag = nullptr;
    bool ok = false;
    while (queue.Next(&tag, &ok)) {
        auto* call = static_cast<AsyncCall*>(tag);
        if (ok && call->status.ok()) {
            std::cout << "Ответ " << call->number << ": "
                      << call->reply.message() << '\n';
        } else {
            std::cerr << "Ошибка асинхронного запроса " << call->number << '\n';
        }
        if (++completed == count) break;
    }
    queue.Shutdown();
    while (queue.Next(&tag, &ok)) { }
    auto async_end = Clock::now();

    auto sync_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        sync_end - sync_start).count();
    auto async_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        async_end - async_start).count();

    std::cout << "\nВремя для " << count << " запросов:\n";
    std::cout << "Последовательно: " << sync_ms << " мс\n";
    std::cout << "Асинхронно:      " << async_ms << " мс\n";
    std::cout << "Время зависит от компьютера, сети и нагрузки.\n";
    return completed == count ? 0 : 1;
}
