#pragma once

#include "../common/dispatcher.hpp"
#include "../client/rpc_client.hpp"
#include "rpc_router.hpp"
#include "rpc_registry.hpp"
#include "rpc_topic.hpp"

namespace bitrpc {
    namespace server {
        // Registry endpoint for provider registration and service discovery.
        class RegistryServer {
            public:
                using ptr = std::shared_ptr<RegistryServer>;
                RegistryServer(int port):
                    _pd_manager(std::make_shared<PDManager>()),
                    _dispatcher(std::make_shared<bitrpc::Dispatcher>())
                {
                    auto service_cb = std::bind(&PDManager::onServiceRequest, _pd_manager.get(),
                        std::placeholders::_1, std::placeholders::_2);
                    _dispatcher->registerHandler<ServiceRequest>(MType::REQ_SERVICE, service_cb);


                    _server = bitrpc::ServerFactory::create(port);
                    auto message_cb = std::bind(&bitrpc::Dispatcher::onMessage, _dispatcher.get(),
                        std::placeholders::_1, std::placeholders::_2);
                    _server->setMessageCallback(message_cb);

                    auto close_cb = std::bind(&RegistryServer::onConnShutdown, this, std::placeholders::_1);
                    _server->setCloseCallback(close_cb);
                }
                void start() {
                    _server->start();
                }
            private:
                void onConnShutdown(const BaseConnection::ptr &conn) {
                    _pd_manager->onConnShutdown(conn);
                }
            private:
                PDManager::ptr _pd_manager;
                Dispatcher::ptr _dispatcher;
                BaseServer::ptr _server;
        };

        class RpcServer {
            public:
                using ptr = std::shared_ptr<RpcServer>;
                // access_addr is the externally reachable provider address.
                // registry_server_addr is used only when registration is enabled.
                RpcServer(const Address &access_addr,
                    bool enableRegistry = false,
                    const Address &registry_server_addr = Address()):
                    _enableRegistry(enableRegistry),
                    _access_addr(access_addr),
                    _router(std::make_shared<bitrpc::server::RpcRouter>()),
                    _dispatcher(std::make_shared<bitrpc::Dispatcher>()) {

                    if (enableRegistry) {
                        _reg_client = std::make_shared<client::RegistryClient>(
                            registry_server_addr.first, registry_server_addr.second);
                    }
                    auto rpc_cb = std::bind(&RpcRouter::onRpcRequest, _router.get(),
                        std::placeholders::_1, std::placeholders::_2);
                    _dispatcher->registerHandler<bitrpc::RpcRequest>(bitrpc::MType::REQ_RPC, rpc_cb);

                    _server = bitrpc::ServerFactory::create(access_addr.second);
                    auto message_cb = std::bind(&bitrpc::Dispatcher::onMessage, _dispatcher.get(),
                        std::placeholders::_1, std::placeholders::_2);
                    _server->setMessageCallback(message_cb);
                }

                void registerMethod(const ServiceDescribe::ptr &service) {
                    if (_enableRegistry) {
                        _reg_client->registryMethod(service->method(), _access_addr);
                    }
                    _router->registerMethod(service);
                }
                void start() {
                    _server->start();
                }
            private:
                bool _enableRegistry;
                Address _access_addr;
                client::RegistryClient::ptr _reg_client;
                RpcRouter::ptr _router;
                Dispatcher::ptr _dispatcher;
                BaseServer::ptr _server;
        };

        class TopicServer {
            public:
                using ptr = std::shared_ptr<TopicServer>;
                TopicServer(int port):
                    _topic_manager(std::make_shared<TopicManager>()),
                    _dispatcher(std::make_shared<bitrpc::Dispatcher>())
                {
                    auto topic_cb = std::bind(&TopicManager::onTopicRequest, _topic_manager.get(),
                        std::placeholders::_1, std::placeholders::_2);
                    _dispatcher->registerHandler<TopicRequest>(MType::REQ_TOPIC, topic_cb);

                    _server = bitrpc::ServerFactory::create(port);
                    auto message_cb = std::bind(&bitrpc::Dispatcher::onMessage, _dispatcher.get(),
                        std::placeholders::_1, std::placeholders::_2);
                    _server->setMessageCallback(message_cb);

                    auto close_cb = std::bind(&TopicServer::onConnShutdown, this, std::placeholders::_1);
                    _server->setCloseCallback(close_cb);
                }
                void start() {
                    _server->start();
                }
            private:
                void onConnShutdown(const BaseConnection::ptr &conn) {
                    _topic_manager->onShutdown(conn);
                }
            private:
                TopicManager::ptr _topic_manager;
                Dispatcher::ptr _dispatcher;
                BaseServer::ptr _server;
        };
    }
}
