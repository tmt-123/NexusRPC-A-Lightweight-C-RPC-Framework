#include "../../client/rpc_client.hpp"

int main()
{
    auto client = std::make_shared<bitrpc::client::TopicClient>("127.0.0.1", 7070);
    bool ret = client->create("hello");
    if (ret == false) {
        ELOG("创建主题失败！");
    }
    for (int i = 0; i < 10; i++) {
        client->publish("hello", "Hello World-" + std::to_string(i));
    }
    client->shutdown();
    return 0;
}
