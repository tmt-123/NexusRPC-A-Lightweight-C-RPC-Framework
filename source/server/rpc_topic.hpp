#pragma once
#include "../common/net.hpp"
#include "../common/message.hpp"
#include <unordered_set>

namespace bitrpc {
    namespace server {
        class TopicManager {
            public:
                using ptr = std::shared_ptr<TopicManager>;
                TopicManager() {}
                void onTopicRequest(const BaseConnection::ptr &conn, const TopicRequest::ptr &msg) {
                    TopicOptype topic_optype = msg->optype();
                    bool ret = true;
                    switch(topic_optype){
                        case TopicOptype::TOPIC_CREATE: topicCreate(conn, msg); break;
                        case TopicOptype::TOPIC_REMOVE: topicRemove(conn, msg); break;
                        case TopicOptype::TOPIC_SUBSCRIBE: ret = topicSubscribe(conn, msg); break;
                        case TopicOptype::TOPIC_CANCEL: topicCancel(conn, msg); break;
                        case TopicOptype::TOPIC_PUBLISH: ret = topicPublish(conn, msg); break;
                        default:  return errorResponse(conn, msg, RCode::RCODE_INVALID_OPTYPE);
                    }
                    if (!ret) return errorResponse(conn, msg, RCode::RCODE_NOT_FOUND_TOPIC);
                    return topicResponse(conn, msg);
                }
                // Remove subscription state associated with a closed connection.
                void onShutdown(const BaseConnection::ptr &conn) {
                    std::vector<Topic::ptr> topics;
                    Subscriber::ptr subscriber;
                    {
                        std::unique_lock<std::mutex> lock(_mutex);
                        auto it = _subscribers.find(conn);
                        if (it == _subscribers.end()) {
                            return;
                        }
                        subscriber = it->second;
                        for (auto &topic_name : subscriber->topics) {
                            auto topic_it = _topics.find(topic_name);
                            if (topic_it == _topics.end()) continue;
                            topics.push_back(topic_it->second);
                        }
                        _subscribers.erase(it);
                    }
                    for (auto &topic : topics) {
                        topic->removeSubscriber(subscriber);
                    }
                }
            private:
                void errorResponse(const BaseConnection::ptr &conn, const TopicRequest::ptr &msg, RCode rcode) {
                    auto msg_rsp = MessageFactory::create<TopicResponse>();
                    msg_rsp->setId(msg->rid());
                    msg_rsp->setMType(MType::RSP_TOPIC);
                    msg_rsp->setRCode(rcode);
                    conn->send(msg_rsp);
                }
                void topicResponse(const BaseConnection::ptr &conn, const TopicRequest::ptr &msg) {
                    auto msg_rsp = MessageFactory::create<TopicResponse>();
                    msg_rsp->setId(msg->rid());
                    msg_rsp->setMType(MType::RSP_TOPIC);
                    msg_rsp->setRCode(RCode::RCODE_OK);
                    conn->send(msg_rsp);
                }
                void topicCreate(const BaseConnection::ptr &conn, const TopicRequest::ptr &msg) {
                    std::unique_lock<std::mutex> lock(_mutex);
                    std::string topic_name = msg->topicKey();
                    auto topic = std::make_shared<Topic>(topic_name);
                    _topics.insert(std::make_pair(topic_name, topic));
                }
                void topicRemove(const BaseConnection::ptr &conn, const TopicRequest::ptr &msg) {
                    std::string topic_name = msg->topicKey();
                    std::unordered_set<Subscriber::ptr> subscribers;
                    {
                        std::unique_lock<std::mutex> lock(_mutex);
                        auto it = _topics.find(topic_name);
                        if (it == _topics.end()) {
                            return;
                        }
                        subscribers = it->second->subscribers;
                        _topics.erase(it);
                    }
                    for (auto &subscriber : subscribers) {
                        subscriber->removeTopic(topic_name);
                    }
                }
                bool topicSubscribe(const BaseConnection::ptr &conn, const TopicRequest::ptr &msg) {
                    Topic::ptr topic;
                    Subscriber::ptr subscriber;
                    {
                        std::unique_lock<std::mutex> lock(_mutex);
                        auto topic_it = _topics.find(msg->topicKey());
                        if (topic_it == _topics.end()) {
                            return false;
                        }
                        topic = topic_it->second;
                        auto sub_it = _subscribers.find(conn);
                        if (sub_it != _subscribers.end()) {
                            subscriber = sub_it->second;
                        }else {
                            subscriber = std::make_shared<Subscriber>(conn);
                            _subscribers.insert(std::make_pair(conn, subscriber));
                        }
                    }
                    topic->appendSubscriber(subscriber);
                    subscriber->appendTopic(msg->topicKey());
                    return true;
                }
                void topicCancel(const BaseConnection::ptr &conn, const TopicRequest::ptr &msg) {
                    Topic::ptr topic;
                    Subscriber::ptr subscriber;
                    {
                        std::unique_lock<std::mutex> lock(_mutex);
                        auto topic_it = _topics.find(msg->topicKey());
                        if (topic_it != _topics.end()) {
                            topic =  topic_it->second;
                        }
                        auto sub_it = _subscribers.find(conn);
                        if (sub_it != _subscribers.end()) {
                            subscriber = sub_it->second;
                        }
                    }
                    if (subscriber) subscriber->removeTopic(msg->topicKey());
                    if (topic && subscriber) topic->removeSubscriber(subscriber);
                }
                bool topicPublish(const BaseConnection::ptr &conn, const TopicRequest::ptr &msg) {
                    Topic::ptr topic;
                    {
                        std::unique_lock<std::mutex> lock(_mutex);
                        auto topic_it = _topics.find(msg->topicKey());
                        if (topic_it == _topics.end()) {
                            return false;
                        }
                        topic = topic_it->second;
                    }
                    topic->pushMessage(msg);
                    return true;
                }
            private:
                struct Subscriber {
                    using ptr = std::shared_ptr<Subscriber>;
                    std::mutex _mutex;
                    BaseConnection::ptr conn;
                    std::unordered_set<std::string> topics;

                    Subscriber(const BaseConnection::ptr &c): conn(c) { }
                    void appendTopic(const std::string &topic_name) {
                        std::unique_lock<std::mutex> lock(_mutex);
                        topics.insert(topic_name);
                    }
                    void removeTopic(const std::string &topic_name) {
                        std::unique_lock<std::mutex> lock(_mutex);
                        topics.erase(topic_name);
                    }
                };
                struct Topic {
                    using ptr = std::shared_ptr<Topic>;
                    std::mutex _mutex;
                    std::string topic_name;
                    std::unordered_set<Subscriber::ptr> subscribers;

                    Topic(const std::string &name) : topic_name(name){}
                    void appendSubscriber(const Subscriber::ptr &subscriber) {
                        std::unique_lock<std::mutex> lock(_mutex);
                        subscribers.insert(subscriber);
                    }
                    void removeSubscriber(const Subscriber::ptr &subscriber) {
                        std::unique_lock<std::mutex> lock(_mutex);
                        subscribers.erase(subscriber);
                    }
                    void pushMessage(const BaseMessage::ptr &msg) {
                        std::unique_lock<std::mutex> lock(_mutex);
                        for (auto &subscriber : subscribers) {
                            subscriber->conn->send(msg);
                        }
                    }
                };
            private:
                std::mutex _mutex;
                std::unordered_map<std::string, Topic::ptr> _topics;
                std::unordered_map<BaseConnection::ptr, Subscriber::ptr> _subscribers;
        };
    }
}
