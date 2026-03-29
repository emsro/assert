#pragma once

#include "../asrtl/flat_tree.h"

#include <filesystem>
#include <iosfwd>
#include <map>
#include <nlohmann/json_fwd.hpp>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace asrtio
{

struct param_config
{
        asrtl_flat_tree tree{};

        param_config()
        {
                asrtl_flat_tree_init( &tree, asrtl_default_allocator(), 4, 32 );
        }
        ~param_config()
        {
                if ( tree.alloc.free )
                        asrtl_flat_tree_deinit( &tree );
        }
        param_config( param_config const& )            = delete;
        param_config& operator=( param_config const& ) = delete;
        param_config( param_config&& o ) noexcept
            : tree( o.tree )
            , wildcard( std::move( o.wildcard ) )
            , tests( std::move( o.tests ) )
        {
                o.tree = {};
        }
        param_config& operator=( param_config&& ) = delete;

        std::vector< asrtl_flat_id >                          wildcard;
        std::map< std::string, std::vector< asrtl_flat_id > > tests;

        struct run_view
        {
                bool                             skip = false;
                std::span< asrtl_flat_id const > roots;
        };

        run_view runs_for( std::string const& name ) const;
};

std::optional< param_config > param_config_from_json( nlohmann::json const& j );
std::optional< param_config > param_config_from_stream( std::istream& in );
std::optional< param_config > param_config_from_file( std::filesystem::path const& path );

}  // namespace asrtio
