#pragma once

#include "../asrtlpp/task.hpp"
#include "./reactor.hpp"

#include <ecor/ecor.hpp>

namespace asrt
{


struct task_test
{
        task_test( task_ctx& ctx )
          : ctx( ctx )
        {
        }

        template < typename T >
        auto& query( T&& q )
        {
                return ctx.query( (T&&) q );
        }

private:
        task_ctx& ctx;
};

using ecor::suspend;
using ecor::with_error;

// XXX: do some form of type erasure to reduce code ize.
template < typename T >
struct task_unit : asrtr_test
{

        struct recv
        {
                using receiver_concept = ecor::receiver_t;

                record* rec;

                void set_value() { rec->state = ASRTR_TEST_PASS; }
                void set_error( ecor::task_error ) { rec->state = ASRTR_TEST_FAIL; }
                void set_error( test_fail_t ) { rec->state = ASRTR_TEST_FAIL; }
                void set_error( asrt::status ) { rec->state = ASRTR_TEST_ERROR; }
                void set_stopped() { rec->state = ASRTR_TEST_FAIL; }
        };

        task_unit( T def )
          : def( std::move( def ) )
        {
                asrtr_test_init( this, def.name, static_cast< task_unit* >( this ), task_unit::cb );
        }

        static asrtl_status cb( record* rec )
        {
                auto& self = *static_cast< task_unit* >( rec->inpt->test_ptr );

                if ( rec->state == ASRTR_TEST_INIT ) {
                        rec->state = ASRTR_TEST_RUNNING;
                        self.op    = self.def.exec().connect( recv{ rec } );
                        self.op.start();
                }

                return ASRTL_SUCCESS;
        }

private:
        T def;
        // XXX: note that this being here is waste of memory - it is needed only per active test
        ecor::connect_type< task< void >, recv > op;
};

}  // namespace asrt
