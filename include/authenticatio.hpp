//
// Created by Supradeep Chitumalla on 06/09/25.
//

#ifndef AUTHENTICATIO_H
#define AUTHENTICATIO_H
#include "config.hpp"
#include <string>
#include <cpprest/http_client.h>
namespace derbit {
class Authentication {
public:
    explicit Authentication(deribit::Config &config);
    bool authenticate();

    std::string get_access_token();
    bool is_authenticated();
    void refresh_token();
private:
    deribit::Config& config_;
    bool is_authenticated_;
    web::http::client::http_client client_;
    };
}
#endif //AUTHENTICATIO_H
