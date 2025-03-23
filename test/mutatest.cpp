#include "../asrtcpp/controller.hpp"
#include "../asrtcpp/fmt.hpp"
#include "../asrtl/ecode_to_str.h"
#include "../asrtlpp/fmt.hpp"
#include "../asrtrpp/fmt.hpp"
#include "../asrtrpp/reactor.hpp"

#include <iostream>
#include <source_location>
#include <sstream>


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

void print_msg( std::ostream& os, asrtl::source s, asrtl::source t, std::span< std::byte > buff )
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

struct gene_handler
{
        std::ostream&      os;
        asrtc::controller& c;
        asrtr::reactor&    r;
        checker            check{ os };

        void operator()( _tick const& t )
        {
                os << "T" << std::endl;
                for ( int i = 0; i < t.i; i++ ) {
                        check >> c.tick();
                        std::byte b[64];
                        check >> r.tick( b );
                }
        }

        void operator()( _d const& )
        {
                os << "D" << std::endl;
                check >> c.query_desc( [&]( std::string_view ) {
                        return ASRTC_SUCCESS;
                } );
        }

        void operator()( _tc const& )
        {
                os << "TC" << std::endl;
                check >> c.query_test_count( [&]( uint32_t ) {
                        return ASRTC_SUCCESS;
                } );
        }

        void operator()( _ti const& )
        {
                os << "TI" << std::endl;
                check >> c.query_test_info( 0, [&]( std::string_view ) {
                        return ASRTC_SUCCESS;
                } );
        }

        void operator()( _ex const& )
        {
                os << "EX" << std::endl;
                check >> c.exec_test( 0, [&]( asrtc::result const& ) {
                        return ASRTC_SUCCESS;
                } );
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
        auto                     r_cb = [&]( asrtl::chann_id, std::span< std::byte > buff ) {
                print_msg( os, ASRTL_REACTOR, ASRTL_CONTROLLER, buff );
                // XXX: maybe create C++ alternative of the dispatch?
                check >> c->node()->recv_cb( c->node()->recv_ptr, asrtl::cnv( buff ) );
                return ASRTL_SUCCESS;
        };
        asrtr::reactor r{ r_cb, "Test reactor" };
        noop_test      t1;
        r.add_test( t1 );

        c.emplace(
            [&]( asrtl::chann_id, std::span< std::byte > buff ) {
                    print_msg( os, ASRTL_CONTROLLER, ASRTL_REACTOR, buff );
                    check >> r.node()->recv_cb( r.node()->recv_ptr, asrtl::cnv( buff ) );
                    return ASRTL_SUCCESS;
            },
            [&]( asrtl::source s, asrtl::ecode ec ) {
                    os << std::format( "({}) ", s );
                    os << asrtl_ecode_to_str( (enum asrtl_ecode) ec ) << std::endl;
                    return ASRTC_SUCCESS;
            } );

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
