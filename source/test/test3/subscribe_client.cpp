#include "../../client/rpc_client.hpp"

void callback(const std::string &key, const std::string &msg) {
    ILOG("%s 主题收到推送过来的消息： %s", key.c_str(), msg.c_str());
}

int main()
{
    auto client = std::make_shared<bitrpc::client::TopicClient>("127.0.0.1", 7070);
    bool ret = client->create("hello");
    if (ret == false) {
        ELOG("创建主题失败！");
    }
    ret = client->subscribe("hello", callback);
    std::this_thread::sleep_for(std::chrono::seconds(10));
    client->shutdown();
    return 0;
}
