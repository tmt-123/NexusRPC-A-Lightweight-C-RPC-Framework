#pragma once
#include "../common/net.hpp"
#include "../common/message.hpp"

namespace bitrpc {
    namespace server {
        enum class VType {
            BOOL = 0,
            INTEGRAL,
            NUMERIC,
            STRING,
            ARRAY,
            OBJECT,
        };
        class ServiceDescribe {
            public:
                using ptr = std::shared_ptr<ServiceDescribe>;
                using ServiceCallback = std::function<void(const Json::Value&, Json::Value &)>;
                using ParamsDescribe = std::pair<std::string, VType>;
                ServiceDescribe(std::string &&mname, std::vector<ParamsDescribe> &&desc,
                    VType vtype, ServiceCallback &&handler) :
                    _method_name(std::move(mname)),_callback(std::move(handler)),
                    _params_desc(std::move(desc)), _return_type(vtype)
                {}
                const std::string &method() { return _method_name; }
                // Validate request parameters against the registered method schema.
                bool paramCheck(const Json::Value &params){
                    for (auto &desc : _params_desc) {
                        if (params.isMember(desc.first) == false) {
                            ELOG("参数字段完整性校验失败！%s 字段缺失！", desc.first.c_str());
                            return false;
                        }
                        if (check(desc.second, params[desc.first]) == false) {
                            ELOG("%s 参数类型校验失败！", desc.first.c_str());
                            return false;
                        }
                    }
                    return true;
                }
                bool call(const Json::Value &params, Json::Value &result) {
                    _callback(params, result);
                    if (rtypeCheck(result) == false) {
                        ELOG("回调处理函数中的响应信息校验失败！");
                        return false;
                    }
                    return true;
                }
            private:
                bool rtypeCheck(const Json::Value &val) {
                    return check(_return_type, val);
                }
                bool check(VType vtype, const Json::Value &val) {
                    switch(vtype) {
                        case VType::BOOL : return val.isBool();
                        case VType::INTEGRAL : return val.isIntegral();
                        case VType::NUMERIC : return val.isNumeric();
                        case VType::STRING : return val.isString();
                        case VType::ARRAY : return val.isArray();
                        case VType::OBJECT : return val.isObject();
                    }
                    return false;
                }
            private:
                std::string _method_name;
                ServiceCallback _callback;
                std::vector<ParamsDescribe> _params_desc;
                VType _return_type;
        };

        class SDescribeFactory {
            public:
                void setMethodName(const std::string &name) {
                    _method_name = name;
                }
                void setReturnType(VType vtype) {
                    _return_type = vtype;
                }
                void setParamsDesc(const std::string &pname, VType vtype) {
                    _params_desc.push_back(ServiceDescribe::ParamsDescribe(pname, vtype));
                }
                void setCallback(const ServiceDescribe::ServiceCallback &cb) {
                    _callback = cb;
                }
                ServiceDescribe::ptr build() {
                    return std::make_shared<ServiceDescribe>(std::move(_method_name),
                        std::move(_params_desc), _return_type, std::move(_callback));
                }
            private:
                std::string _method_name;
                ServiceDescribe::ServiceCallback _callback;
                std::vector<ServiceDescribe::ParamsDescribe> _params_desc;
                VType _return_type;
        };

        class ServiceManager {
            public:
                using ptr = std::shared_ptr<ServiceManager>;
                void insert(const ServiceDescribe::ptr &desc)  {
                    std::unique_lock<std::mutex> lock(_mutex);
                    _services.insert(std::make_pair(desc->method(), desc));
                }
                ServiceDescribe::ptr select(const std::string &method_name) {
                    std::unique_lock<std::mutex> lock(_mutex);
                    auto it = _services.find(method_name);
                    if (it == _services.end()) {
                        return ServiceDescribe::ptr();
                    }
                    return it->second;
                }
                void remove(const std::string &method_name) {
                    std::unique_lock<std::mutex> lock(_mutex);
                    _services.erase(method_name);
                }
            private:
                std::mutex _mutex;
                std::unordered_map<std::string, ServiceDescribe::ptr> _services;
        };

        class RpcRouter {
            public:
                using ptr = std::shared_ptr<RpcRouter>;
                RpcRouter(): _service_manager(std::make_shared<ServiceManager>()){}
                // Dispatcher entry point for RPC requests.
                void onRpcRequest(const BaseConnection::ptr &conn, RpcRequest::ptr &request){
                    // Resolve the method and validate its request payload.
                    auto service = _service_manager->select(request->method());
                    if (service.get() == nullptr) {
                        ELOG("%s 服务未找到！", request->method().c_str());
                        return response(conn, request, Json::Value(), RCode::RCODE_NOT_FOUND_SERVICE);
                    }
                    if (service->paramCheck(request->params()) == false) {
                        ELOG("%s 服务参数校验失败！", request->method().c_str());
                        return response(conn, request, Json::Value(), RCode::RCODE_INVALID_PARAMS);
                    }
                    Json::Value result;
                    bool ret = service->call(request->params(), result);
                    if (ret == false) {
                        ELOG("%s 服务参数校验失败！", request->method().c_str());
                        return response(conn, request, Json::Value(), RCode::RCODE_INTERNAL_ERROR);
                    }
                    return response(conn, request, result, RCode::RCODE_OK);
                }
                void registerMethod(const ServiceDescribe::ptr &service) {
                    return _service_manager->insert(service);
                }
            private:
                void response(const BaseConnection::ptr &conn,
                    const RpcRequest::ptr &req,
                    const Json::Value &res, RCode rcode) {
                    auto msg = MessageFactory::create<RpcResponse>();
                    msg->setId(req->rid());
                    msg->setMType(bitrpc::MType::RSP_RPC);
                    msg->setRCode(rcode);
                    msg->setResult(res);
                    conn->send(msg);
                }
            private:
                ServiceManager::ptr _service_manager;
        };

    }
}
