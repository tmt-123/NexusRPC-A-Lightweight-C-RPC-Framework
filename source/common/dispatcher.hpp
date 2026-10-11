#pragma once
#include "net.hpp"
#include "message.hpp"

namespace bitrpc {
    class Callback {
        public:
            using ptr = std::shared_ptr<Callback>;
            virtual void onMessage(const BaseConnection::ptr &conn, BaseMessage::ptr &msg) = 0;
    };

    template<typename T>
    class CallbackT : public Callback{
        public:
            using ptr = std::shared_ptr<CallbackT<T>>;
            using MessageCallback = std::function<void(const BaseConnection::ptr &conn, std::shared_ptr<T> &msg)>;
            CallbackT(const MessageCallback &handler):_handler(handler) { }
            void onMessage(const BaseConnection::ptr &conn, BaseMessage::ptr &msg) override {
                auto type_msg = std::dynamic_pointer_cast<T>(msg);
                _handler(conn, type_msg);
            }
        private:
            MessageCallback _handler;
    };
    class Dispatcher {
        public:
            using ptr = std::shared_ptr<Dispatcher>;
            template<typename T>
            void registerHandler(MType mtype, const typename CallbackT<T>::MessageCallback &handler) {
                std::unique_lock<std::mutex> lock(_mutex);
                auto cb = std::make_shared<CallbackT<T>>(handler);
                _handlers.insert(std::make_pair(mtype, cb));
            }
            void onMessage(const BaseConnection::ptr &conn, BaseMessage::ptr &msg) {
                // Dispatch the message to the handler registered for its wire type.
                std::unique_lock<std::mutex> lock(_mutex);
                auto it = _handlers.find(msg->mtype());
                if (it != _handlers.end()) {
                    return it->second->onMessage(conn, msg);
                }
                // Unknown message types are rejected instead of being silently ignored.
                ELOG("收到未知类型的消息: %d！", msg->mtype());
                conn->shutdown();
            }
        private:
            std::mutex _mutex;
            std::unordered_map<MType, Callback::ptr> _handlers;
    };
}
