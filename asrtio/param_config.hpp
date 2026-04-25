/// Permission to use, copy, modify, and/or distribute this software for any
/// purpose with or without fee is hereby granted.
///
/// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
/// REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
/// AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
/// INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
/// LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
/// OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
/// PERFORMANCE OF THIS SOFTWARE.
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
        asrt_flat_tree tree{};

        param_config() { asrt_flat_tree_init( &tree, asrt_default_allocator(), 4, 32 ); }
        ~param_config()
        {
                if ( tree.alloc.free )
                        asrt_flat_tree_deinit( &tree );
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
