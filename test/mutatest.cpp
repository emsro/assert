#include "../asrtcpp/controller.hpp"
#include "../asrtcpp/fmt.hpp"
#include "../asrtl/ecode_to_str.h"
#include "../asrtl/log.h"
#include "../asrtlpp/fmt.hpp"
#include "../asrtrpp/fmt.hpp"
#include "../asrtrpp/reactor.hpp"

#include <iostream>
#include <source_location>
#include <sstream>
#include <string_view>
#include <variant>
#include <vector>

extern "C" {
ASRTL_DEFINE_GPOS_LOG()
}


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

using asrtl::opt;

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

struct _ex
{
        // exec
} EX;

template < typename T >
struct _error
{
        T sub;
};
template <>
struct _error< void >
{
};
_error< void > E;

template < typename T >
_error< T > operator+( T& x, _error< void > E )
{
        return { x };
}

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
        std::vector< std::variant<
            _d,
            _tc,
            _ti,
            _ex,
            _tick,
            _error< _d >,
            _error< _ti >,
            _error< _tc >,
            _error< _ex > > >
            genes;
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

void print_msg( std::ostream& os, asrtl::source s, asrtl::source t, asrtl::rec_span const* buff )
{
        os << std::format( "{} -> {}\t", s, t );
        int i = 0;
        for ( asrtl::rec_span const* seg = buff; seg; seg = seg->next )
                for ( uint8_t const* b = seg->b; b < seg->e; ++b, ++i ) {
                        if ( i == 0 ) {
                        } else if ( i % 2 == 1 )
                                os << ':';
                        else if ( i % 2 == 0 )
                                os << '|';
                        os << std::format( "{:02x}", *b );
                }
        os << std::endl;
}

struct noop_test
{
        char const* name()
        {
                return "noop_test";
        }

        asrtr::status operator()( asrtr::record& rec )
        {
                rec.state = ASRTR_TEST_PASS;
                return ASRTR_SUCCESS;
        }
};

struct gene_handler
{
        std::ostream&      os;
        asrtc::controller& c;
        asrtr::reactor&    r;
        checker            check{ os };
        uint32_t           t = 1;

        void operator()( _tick const& t_ )
        {
                os << "T" << std::endl;
                for ( int i = 0; i < t_.i; i++ ) {
                        check >> asrtl_chann_tick( c.node(), t++ );
                        check >> asrtl_chann_tick( r.node(), t++ );
                }
        }

        void operator()( _d const& )
        {
                os << "D" << std::endl;
                check >> c.query_desc( [&]( asrtc::status s, std::string_view ) -> asrtl::status {
                        check >> s;
                        return ASRTL_SUCCESS;
                }, 1000 );
        }

        void operator()( _tc const& )
        {
                os << "TC" << std::endl;
                check >> c.query_test_count( [&]( asrtc::status s, uint32_t ) -> asrtl::status {
                        check >> s;
                        return ASRTL_SUCCESS;
                }, 1000 );
        }

        void operator()( _ti const& )
        {
                os << "TI" << std::endl;
                check >> c.query_test_info( 0, [&]( asrtc::status s, uint16_t, std::string_view ) -> asrtl::status {
                        check >> s;
                        return ASRTL_SUCCESS;
                }, 1000 );
        }

        void operator()( _ex const& )
        {
                os << "EX" << std::endl;
                check >> c.exec_test( 0, [&]( asrtc::status s, asrtc::result const& ) -> asrtl::status {
                        check >> s;
                        return ASRTL_SUCCESS;
                }, 1000 );
        }

        template < typename T >
        void operator()( _error< T > const& e )
        {
                os << "E" << std::endl;
                std::stringstream ss;
                gene_handler      sub{ ss, c, r };
                try {
                        sub( e.sub );
                }
                catch ( abort_test const& ) {
                        return;
                }
                os << "Gene passed" << std::endl;
                throw abort_test{};
        }
};

void exec( std::ostream& os, test_case const& tc )
{

        checker                  check{ os };
        opt< asrtc::controller > c;

        auto flatten = []( asrtl::rec_span const* buff ) {
                std::vector< uint8_t > flat;
                for ( auto const* seg = buff; seg; seg = seg->next )
                        flat.insert( flat.end(), seg->b, seg->e );
                return flat;
        };

        auto r_cb = [&]( asrtl::chann_id,
                        asrtl::rec_span*   buff,
                        asrtl_send_done_cb done_cb,
                        void*              done_ptr ) {
                print_msg( os, ASRTL_REACTOR, ASRTL_CONTROLLER, buff );
                auto              flat = flatten( buff );
                enum asrtl_status st =
                    asrtl_chann_recv( c->node(), asrtl::cnv( std::span{ flat } ) );
                check >> st;
                if ( done_cb )
                        done_cb( done_ptr, st );
                return st;
        };
        asrtr::reactor           r{ r_cb, "Test reactor" };
        asrtr::unit< noop_test > t1{};
        r.add_test( t1 );

        auto c_send = [&]( asrtl_chann_id,
                          asrtl_rec_span*    buff,
                          asrtl_send_done_cb done_cb,
                          void*              done_ptr ) {
                print_msg( os, ASRTL_CONTROLLER, ASRTL_REACTOR, buff );
                auto              flat = flatten( buff );
                enum asrtl_status st =
                    asrtl_chann_recv( r.node(), asrtl::cnv( std::span{ flat } ) );
                check >> st;
                if ( done_cb )
                        done_cb( done_ptr, st );
                return st;
        };

        c.emplace( c_send, [&]( asrtl::source s, asrtl::ecode ec ) {
                os << std::format( "({}) ", s );
                os << asrtl_ecode_to_str( (enum asrtl_ecode) ec ) << std::endl;
                return ASRTC_SUCCESS;
        } );

        auto s = c->start( [&]( asrtc::status s ) -> asrtl::status {
                check >> s;
                return ASRTL_SUCCESS;
        }, 1000 );
        if ( s != ASRTC_SUCCESS )
                os << std::format( "Controller start failed: {}\n", asrtc_status_to_str( s ) );

        gene_handler gh{ os, *c, r };
        for ( auto const& gene : tc.genes )
                std::visit( gh, gene );
}

int main( int argc, char* argv[] )
{
        std::vector< test_case > test_cases = {
            test_case{ .genes = { T[4], D, T[2], TC, T[2], TI, T[2], EX, T[4] } },
            test_case{ .genes = { T[2], TC, T[2], EX, T[3], D + E, T[2] } },
            test_case{ .genes = { TI + E, D + E, T[3], EX, TC + E, T[42], D, T[2], D } },
            test_case{ .genes = { T[2], EX, TI + E, T[2] } },
            test_case{ .genes = { T[1], D + E, TC + E, TI + E, EX + E, T[1], D, T[1] } },
            test_case{ .genes = { EX + E, T[3], TC, T[5], D, T[1], EX + E, TI + E } },
            test_case{
                .genes = { T[3], D, D + E, TC + E, TC + E, TI + E, TI + E, EX + E, EX + E, T[1] } },
            test_case{ .genes = { T[10], TC, D + E, TI + E, EX + E, T[5] } },
            test_case{ .genes = { T[25], D, TI + E, TC + E, D + E, T[15], EX, T[30] } },
            test_case{ .genes = { D + E, T[4], TC, EX + E, T[69], D, TC + E, D + E, T[3] } } };

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
                                  << tc.sl.line() << std::endl;
                        std::cout << ss.str();
                        std::abort();
                }
        }
}
