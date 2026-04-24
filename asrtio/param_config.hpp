#pragma once

#include "../asrtl/flat_tree.h"
#include "../asrtlpp/flat_type_traits.hpp"

#include <filesystem>
#include <iosfwd>
#include <map>
#include <memory>
#include <nlohmann/json_fwd.hpp>
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
        param_config( param_config&& )                 = delete;
        param_config& operator=( param_config&& )      = delete;

        std::vector< asrt::flat_id >                          wildcard;
        std::map< std::string, std::vector< asrt::flat_id > > tests;

        struct run_view
        {
                bool                             skip = false;
                std::span< asrt::flat_id const > roots;
        };

        run_view runs_for( std::string const& name ) const;
};

std::unique_ptr< param_config > param_config_from_json( nlohmann::json const& j );
std::unique_ptr< param_config > param_config_from_stream( std::istream& in );
std::unique_ptr< param_config > param_config_from_file( std::filesystem::path const& path );

}  // namespace asrtio
