#pragma once

#include "../asrtlpp/sender.hpp"
#include "../asrtr/param.h"

#include <cstdint>

namespace asrtr
{

struct param_client
{
        template < typename CB >
        param_client( asrtl_node* prev, CB& send_cb, asrtl_span msg_buffer )
        {
                std::ignore = asrtr_param_client_init(
                    &client_, prev, asrtl::make_sender( send_cb ), msg_buffer );
        }

        param_client( param_client&& )      = delete;
        param_client( param_client const& ) = delete;

        asrtl_node* node()
        {
                return &client_.node;
        }

        [[nodiscard]] bool ready() const
        {
                return client_.ready != 0;
        }

        [[nodiscard]] asrtl_flat_id root_id() const
        {
                return asrtr_param_client_root_id( &client_ );
        }

        [[nodiscard]] asrtl_status query(
            asrtl_flat_id           node_id,
            asrtl_param_response_cb response_cb,
            void*                   response_cb_ptr,
            asrtr_param_error_cb    error_cb,
            void*                   error_ptr )
        {
                return asrtr_param_client_query(
                    &client_, node_id, response_cb, response_cb_ptr, error_cb, error_ptr );
        }

        asrtl_status tick()
        {
                return asrtr_param_client_tick( &client_ );
        }

        ~param_client()
        {
                asrtr_param_client_deinit( &client_ );
        }

private:
        asrtr_param_client client_;
};

}  // namespace asrtr
