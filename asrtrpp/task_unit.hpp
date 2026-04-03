#pragma once

#include "../asrtlpp/task.hpp"
#include "./reactor.hpp"

#include <ecor/ecor.hpp>

namespace asrtr
{

enum task_error
{
        test_fail,
        test_error
};

template < typename T >
using task     = asrtl::task< T, task_error >;
using task_ctx = asrtl::task_ctx;

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

                void set_value()
                {
                        rec->state = ASRTR_TEST_PASS;
                }
                void set_error( ecor::task_error )
                {
                        rec->state = ASRTR_TEST_FAIL;
                }
                // XXX: should this be an error?
                void set_error( task_error s )
                {
                        switch ( s ) {
                        case task_error::test_fail:
                                rec->state = ASRTR_TEST_FAIL;
                                break;
                        case task_error::test_error:
                                rec->state = ASRTR_TEST_ERROR;
                                break;
                        }
                }
                void set_stopped()
                {
                        rec->state = ASRTR_TEST_FAIL;
                }
        };

        task_unit( T def )
          : def( std::move( def ) )
        {
                asrtr_test_init( this, def.name, static_cast< task_unit* >( this ), task_unit::cb );
        }

        static asrtr::status cb( record* rec )
        {
                auto& self = *static_cast< task_unit* >( rec->inpt->test_ptr );

                if ( rec->state == ASRTR_TEST_INIT ) {
                        rec->state = ASRTR_TEST_RUNNING;
                        self.op    = self.def.exec().connect( recv{ rec } );
                        self.op.start();
                }

                return ASRTR_SUCCESS;
        }

private:
        T def;
        // XXX: note that this being here is waste of memory - it is needed only per active test
        ecor::connect_type< task< void >, recv > op;
};

}  // namespace asrtr
