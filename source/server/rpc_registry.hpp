#pragma once
#include "../common/net.hpp"
#include "../common/message.hpp"
#include <set>

namespace bitrpc {
    namespace server {
        class ProviderManager {
            public:
                using ptr = std::shared_ptr<ProviderManager>;
                struct Provider {
                    using ptr = std::shared_ptr<Provider>;
                    std::mutex _mutex;
                    BaseConnection::ptr conn;
                    Address host;
                    std::vector<std::string> methods;
                    Provider(const BaseConnection::ptr &c, const Address &h):
                        conn(c), host(h){}
                    void appendMethod(const std::string &method) {
                        std::unique_lock<std::mutex> lock(_mutex);
                        methods.emplace_back(method);
                    }
                };
                // Associate a provider connection with one of its methods.
                void addProvider(const BaseConnection::ptr &c, const Address &h, const std::string &method) {
                    Provider::ptr provider;
                    {
                        std::unique_lock<std::mutex> lock(_mutex);
                        auto it = _conns.find(c);
                        if (it != _conns.end()) {
                            provider = it->second;
                        }else {
                            provider = std::make_shared<Provider>(c, h);
                            _conns.insert(std::make_pair(c, provider));
                        }
                        auto &providers = _providers[method];
                        providers.insert(provider);
                    }
                    provider->appendMethod(method);
                }
                // Return provider metadata for an established connection.
                Provider::ptr getProvider(const BaseConnection::ptr &c) {
                    std::unique_lock<std::mutex> lock(_mutex);
                    auto it = _conns.find(c);
                    if (it != _conns.end()) {
                        return it->second;
                    }
                    return Provider::ptr();
                }
                // Remove a disconnected provider from all method indexes.
                void delProvider(const BaseConnection::ptr &c) {
                    std::unique_lock<std::mutex> lock(_mutex);
                    auto it = _conns.find(c);
                    if (it == _conns.end()) {
                        return;
                    }
                    for (auto & method : it->second->methods) {
                        auto &providers = _providers[method];
                        providers.erase(it->second);
                    }
                    _conns.erase(it);
                }
                std::vector<Address> methodHosts(const std::string &method) {
                    std::unique_lock<std::mutex> lock(_mutex);
                    auto it = _providers.find(method);
                    if (it == _providers.end()) {
                        return std::vector<Address>();
                    }
                    std::vector<Address> result;
                    for (auto &provider : it->second) {
                        result.push_back(provider->host);
                    }
                    return result;
                }
            private:
                std::mutex _mutex;
                std::unordered_map<std::string, std::set<Provider::ptr>> _providers;
                std::unordered_map<BaseConnection::ptr, Provider::ptr> _conns;
        };

        class DiscovererManager {
            public:
                using ptr = std::shared_ptr<DiscovererManager>;
                struct Discoverer {
                    using ptr = std::shared_ptr<Discoverer>;
                    std::mutex _mutex;
                    BaseConnection::ptr conn;
                    std::vector<std::string> methods;
                    Discoverer(const BaseConnection::ptr &c) : conn(c){}
                    void appendMethod(const std::string &method) {
                        std::unique_lock<std::mutex> lock(_mutex);
                        methods.push_back(method);
                    }
                };
                // Track the methods observed by each discovery connection.
                Discoverer::ptr addDiscoverer(const BaseConnection::ptr &c, const std::string &method) {
                    Discoverer::ptr discoverer;
                    {
                        std::unique_lock<std::mutex> lock(_mutex);
                        auto it = _conns.find(c);
                        if (it != _conns.end()) {
                            discoverer = it->second;
                        }else {
                            discoverer = std::make_shared<Discoverer>(c);
                            _conns.insert(std::make_pair(c, discoverer));
                        }
                        auto &discoverers = _discoverers[method];
                        discoverers.insert(discoverer);
                    }
                    discoverer->appendMethod(method);
                    return discoverer;
                }
                // Remove discovery state associated with a closed connection.
                void delDiscoverer(const BaseConnection::ptr &c) {
                    std::unique_lock<std::mutex> lock(_mutex);
                    auto it = _conns.find(c);
                    if (it == _conns.end()) {
                        return;
                    }
                    for (auto &method : it->second->methods) {
                        auto &discoverers = _discoverers[method];
                        discoverers.erase(it->second);
                    }
                    _conns.erase(it);
                }
                // Notify interested discovery clients that a provider is online.
                void onlineNotify(const std::string &method, const Address &host) {
                    return notify(method, host, ServiceOptype::SERVICE_ONLINE);
                }
                // Notify interested discovery clients that a provider is offline.
                void offlineNotify(const std::string &method, const Address &host) {
                    return notify(method, host, ServiceOptype::SERVICE_OFFLINE);
                }
            private:
                void notify(const std::string &method, const Address &host, ServiceOptype optype) {
                    std::unique_lock<std::mutex> lock(_mutex);
                    auto it = _discoverers.find(method);
                    if (it == _discoverers.end()) {
                        return;
                    }
                    auto msg_req = MessageFactory::create<ServiceRequest>();
                    msg_req->setId(UUID::uuid());
                    msg_req->setMType(MType::REQ_SERVICE);
                    msg_req->setMethod(method);
                    msg_req->setHost(host);
                    msg_req->setOptype(optype);
                    for (auto &discoverer : it->second) {
                        discoverer->conn->send(msg_req);
                    }
                }
            private:
                std::mutex _mutex;
                std::unordered_map<std::string, std::set<Discoverer::ptr>> _discoverers;
                std::unordered_map<BaseConnection::ptr, Discoverer::ptr> _conns;
        };

        class PDManager {
            public:
                using ptr = std::shared_ptr<PDManager>;
                PDManager():
                    _providers(std::make_shared<ProviderManager>()),
                    _discoverers(std::make_shared<DiscovererManager>())
                {}
                void onServiceRequest(const BaseConnection::ptr &conn, const ServiceRequest::ptr &msg) {
                    // Route registration and discovery requests.
                    ServiceOptype optype = msg->optype();
                    if (optype == ServiceOptype::SERVICE_REGISTRY){
                        ILOG("%s:%d 注册服务 %s", msg->host().first.c_str(), msg->host().second, msg->method().c_str());
                        _providers->addProvider(conn, msg->host(), msg->method());
                        _discoverers->onlineNotify(msg->method(), msg->host());
                        return registryResponse(conn, msg);
                    } else if (optype == ServiceOptype::SERVICE_DISCOVERY){
                        ILOG("客户端要进行 %s 服务发现！", msg->method().c_str());
                        _discoverers->addDiscoverer(conn, msg->method());
                        return discoveryResponse(conn, msg);
                    }else {
                        ELOG("收到服务操作请求，但是操作类型错误！");
                        return errorResponse(conn, msg);
                    }
                }
                void onConnShutdown(const BaseConnection::ptr &conn) {
                    auto provider = _providers->getProvider(conn);
                    if (provider.get() != nullptr) {
                        ILOG("%s:%d 服务下线", provider->host.first.c_str(), provider->host.second);
                        for (auto &method : provider->methods) {
                            _discoverers->offlineNotify(method, provider->host);
                        }
                        _providers->delProvider(conn);
                    }
                    _discoverers->delDiscoverer(conn);
                }
            private:
                void errorResponse(const BaseConnection::ptr &conn, const ServiceRequest::ptr &msg) {
                    auto msg_rsp = MessageFactory::create<ServiceResponse>();
                    msg_rsp->setId(msg->rid());
                    msg_rsp->setMType(MType::RSP_SERVICE);
                    msg_rsp->setRCode(RCode::RCODE_INVALID_OPTYPE);
                    msg_rsp->setOptype(ServiceOptype::SERVICE_UNKNOW);
                    conn->send(msg_rsp);
                }
                void registryResponse(const BaseConnection::ptr &conn, const ServiceRequest::ptr &msg) {
                    auto msg_rsp = MessageFactory::create<ServiceResponse>();
                    msg_rsp->setId(msg->rid());
                    msg_rsp->setMType(MType::RSP_SERVICE);
                    msg_rsp->setRCode(RCode::RCODE_OK);
                    msg_rsp->setOptype(ServiceOptype::SERVICE_REGISTRY);
                    conn->send(msg_rsp);
                }
                void discoveryResponse(const BaseConnection::ptr &conn, const ServiceRequest::ptr &msg) {
                    auto msg_rsp = MessageFactory::create<ServiceResponse>();
                    msg_rsp->setId(msg->rid());
                    msg_rsp->setMType(MType::RSP_SERVICE);
                    msg_rsp->setOptype(ServiceOptype::SERVICE_DISCOVERY);
                    std::vector<Address> hosts = _providers->methodHosts(msg->method());
                    if (hosts.empty()) {
                        msg_rsp->setRCode(RCode::RCODE_NOT_FOUND_SERVICE);
                        return conn->send(msg_rsp);
                    }
                    msg_rsp->setRCode(RCode::RCODE_OK);
                    msg_rsp->setMethod(msg->method());
                    msg_rsp->setHost(hosts);
                    return conn->send(msg_rsp);
                }
            private:
                ProviderManager::ptr _providers;
                DiscovererManager::ptr _discoverers;
        };
    }
}
