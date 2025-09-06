//
// Created by Supradeep Chitumalla on 06/09/25.
//

#include "authentication.hpp"
#include <cpprest/json.h>

namespace  derbit {
    Authentication::Authentication(deribit::Config &config)
    : config_(config)
    ,is_authenticated_(false)
    ,client_(config.BASE_URL)
    {}

    bool Authentication::authenticate() {
        web::uri_builder builder("/public/auth");
        builder.append_query("client_id", config_.client_id)
           .append_query("client_secret", config_.client_secret)
           .append_query("grant_type", "client_credentials");
        try {
            auto response = client_.request(web::http::methods::GET, builder.to_string()).get();

            if (response.status_code() == web::http::status_codes::OK) {
                auto json = response.extract_json().get();
                config_.access_token = json["result"]["access_token"].as_string();
                is_authenticated_ = true;
                return true;
            }
        }catch (const std::exception e) {
            is_authenticated_ = false;
        }
        return false;
    }
    std::string Authentication::get_access_token() const {
        return config_.access_token;
    }

    bool Authentication::is_authenticated() const {
        return is_authenticated_;
    }

    void Authentication::refresh_token() {
        authenticate();
    }

}
