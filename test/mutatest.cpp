
#include "../asrtc/status_to_str.h"
#include "../asrtcpp/controller.hpp"
#include "../asrtl/ecode_to_str.h"
#include "../asrtl/status_to_str.h"
#include "../asrtr/status_to_str.h"
#include "../asrtrpp/reactor.hpp"
#include "../asrtrpp/util.hpp"

#include <iostream>
#include <source_location>
#include <sstream>

template <>
struct std::formatter< enum asrtc_source, char >
{
        template < class ParseContext >
        constexpr auto parse( ParseContext& ctx )
        {
                return ctx.begin();
        }

        auto format( enum asrtc_source status, auto& ctx ) const
        {
                return std::format_to(
                    ctx.out(), "{}", status == ASRTC_CONTROLLER ? "cntr" : "reac" );
        }
};
// XXX: maybe move these somewhere
template <>
struct std::formatter< enum asrtl_status, char >
{
        template < class ParseContext >
        constexpr auto parse( ParseContext& ctx )
        {
                return ctx.begin();
        }

        auto format( enum asrtl_status status, auto& ctx ) const
        {
                return std::format_to( ctx.out(), "{}", asrtl_status_to_str( status ) );
        }
};
template <>
struct std::formatter< enum asrtc_status, char >
{
        template < class ParseContext >
        constexpr auto parse( ParseContext& ctx )
        {
                return ctx.begin();
        }

        auto format( enum asrtc_status status, auto& ctx ) const
        {
                return std::format_to( ctx.out(), "{}", asrtc_status_to_str( status ) );
        }
};

template <>
struct std::formatter< enum asrtr_status, char >
{
        template < class ParseContext >
        constexpr auto parse( ParseContext& ctx )
        {
                return ctx.begin();
        }

        auto format( enum asrtr_status status, auto& ctx ) const
        {
                return std::format_to( ctx.out(), "{}", asrtr_status_to_str( status ) );
        }
};

// XXX: move
template < class... Ts >
struct overloads : Ts...
{
        using Ts::operator()...;
};

decltype( auto ) match( auto&& value, auto&&... lambdas )
{
        return std::visit( overloads{ lambdas... }, std::forward< decltype( value ) >( value ) );
}

using asrtc::opt;

struct _d
{
        // desc
} D;

struct _tc
{
        // test count
} TC;

struct _ti
{
        // test info
} TI;

struct _e
{
        // exec
} E;

struct _ee
{
        // end exec
} Ee;

struct _tick
{
        int i = 1;

        _tick operator[]( int i ) const
        {
                return _tick{ i };
        }
} T;

struct abort_test
{
};

struct test_case
{
        std::source_location sl = std::source_location::current();
        std::vector< std::variant< _d, _tc, _ti, _e, _ee, _tick > > genes;
};

struct checker
{
        std::ostream& os;

        template < typename T >
        struct tok
        {
                tok( T t, std::source_location loc = std::source_location::current() )
                  : v( std::move( t ) )
                  , loc( loc )
                {
                }

                T                    v;
                std::source_location loc;
        };

        void operator>>( tok< asrtc_status > st )
        {
                check( st.v, ASRTC_SUCCESS, st.loc );
        }
        void operator>>( tok< asrtl_status > st )
        {
                check( st.v, ASRTL_SUCCESS, st.loc );
        }
        void operator>>( tok< asrtr_status > st )
        {
                check( st.v, ASRTR_SUCCESS, st.loc );
        }

        void check( auto& x, auto&& y, std::source_location const& loc )
        {
                if ( x == y )
                        return;

                os << std::format(
                    "Check failed {}:{} status: {}\n", loc.file_name(), loc.line(), x );
                throw abort_test{};
        }
};

void print_msg( std::ostream& os, asrtc::source s, asrtc::source t, std::span< std::byte > buff )
{
        os << std::format( "{} -> {}\t", s, t );
        for ( int i = 0; i < buff.size(); ++i ) {
                if ( i == 0 ) {
                } else if ( i % 2 == 1 )
                        os << ':';
                else if ( i % 2 == 0 )
                        os << '|';

                os << std::format( "{:02x}", static_cast< uint8_t >( buff[i] ) );
        }
        os << std::endl;
}

struct noop_test : asrtr::unit< noop_test >
{
        static constexpr char const* desc = "noop";

        asrtr::status operator()()
        {
                return ASRTR_SUCCESS;
        }
};

void exec( std::ostream& os, test_case const& tc )
{
        checker check{ os };

        opt< asrtc::controller > c;
        auto                     r_cb = [&]( asrtl::chann_id, std::span< std::byte > buff ) {
                print_msg( os, ASRTC_REACTOR, ASRTC_CONTROLLER, buff );
                // XXX: maybe create C++ alternative of the dispatch?
                check >> c->node()->recv_cb( c->node()->recv_ptr, asrtr::cnv( buff ) );
                return ASRTL_SUCCESS;
        };
        asrtr::reactor r{ r_cb, "Test reactor" };
        noop_test      t1;
        r.add_test( t1 );

        c.emplace( asrtc::make_controller(
                       [&]( asrtl::chann_id, std::span< std::byte > buff ) {
                               print_msg( os, ASRTC_CONTROLLER, ASRTC_REACTOR, buff );
                               check >> r.node()->recv_cb( r.node()->recv_ptr, asrtr::cnv( buff ) );
                               return ASRTL_SUCCESS;
                       },
                       [&]( asrtc::source s, asrtl::ecode ec ) {
                               os << std::format( "({}) ", s );
                               os << asrtl_ecode_to_str( (enum asrtl_ecode) ec ) << std::endl;
                               return ASRTC_SUCCESS;
                       } )
                       .value() );

        for ( auto const& gene : tc.genes ) {
                match(
                    gene,
                    [&]( _tick const& t ) {
                            os << "T" << std::endl;
                            for ( int i = 0; i < t.i; i++ ) {
                                    check >> c->tick();
                                    std::byte b[64];
                                    check >> r.tick( b );
                            }
                    },
                    [&]( _d const& ) {
                            os << "D" << std::endl;
                            check >> c->query_desc( [&]( std::string_view ) {
                                    return ASRTC_SUCCESS;
                            } );
                    },
                    [&]( _tc const& ) {
                            os << "TC" << std::endl;
                            check >> c->query_test_count( [&]( uint32_t ) {
                                    return ASRTC_SUCCESS;
                            } );
                    },
                    [&]( _ti const& ) {
                            os << "TI" << std::endl;
                            check >> c->query_test_info( 0, [&]( std::string_view ) {
                                    return ASRTC_SUCCESS;
                            } );
                    },
                    [&]( _e const& ) {
                            os << "E" << std::endl;
                            check >> c->exec_test( 0, [&]( asrtc::result const& ) {
                                    return ASRTC_SUCCESS;
                            } );
                    },
                    [&]( _ee const& ) {
                            os << "EE" << std::endl;
                            // XXX: end the test
                    } );
        }
}

int main( int argc, char* argv[] )
{
        std::vector< test_case > test_cases = {
            test_case{
                .genes = { T[4], D, T[2], TC, T[2], TI, T[2], E, T[4], Ee },
            },
            test_case{
                .genes = { T[2], TC, T[2], E, T[3], D, T[2], Ee },
            },
            test_case{
                .genes = { TI, D, T[3], E, TC, T[42], D, Ee },
            },
            test_case{
                .genes = { E, TI, T[2] },
            },
        };

        for ( test_case const& tc : test_cases ) {
                std::stringstream ss;
                try {
                        exec( ss, tc );
                }
                catch ( std::exception& e ) {
                        std::cerr << e.what() << std::endl;
                }
                catch ( abort_test const& ) {
                        std::cout << "Failed test case: " << tc.sl.file_name() << ":"
                                  << tc.sl.line() - 1 << std::endl;
                        std::cout << ss.str();
                        std::abort();
                }
        }
}
