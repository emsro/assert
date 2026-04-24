#include "./param_config.hpp"

#include "../asrtl/log.h"
#include "./util.hpp"

#include <fstream>
#include <memory>
#include <nlohmann/json.hpp>

namespace asrtio
{

param_config::run_view param_config::runs_for( std::string const& name ) const
{
        auto it = tests.find( name );
        if ( it != tests.end() ) {
                if ( it->second.empty() )
                        return { .skip = true, .roots = {} };
                return { .skip = false, .roots = it->second };
        }
        if ( !wildcard.empty() )
                return { .skip = false, .roots = wildcard };
        static constexpr asrt::flat_id no_params[] = { 0 };
        return { .skip = false, .roots = no_params };
}

std::unique_ptr< param_config > param_config_from_json( nlohmann::json const& j )
{
        if ( !j.is_object() ) {
                ASRTL_ERR_LOG( "asrtio", "param config: top level must be a JSON object" );
                return nullptr;
        }

        auto          cfg     = std::make_unique< param_config >();
        asrt::flat_id next_id = 1;

        for ( auto const& [key, val] : j.items() ) {
                // Normalize: bare object → [object]
                std::vector< nlohmann::json const* > entries;
                if ( val.is_object() ) {
                        entries.push_back( &val );
                } else if ( val.is_array() ) {
                        for ( auto const& elem : val ) {
                                if ( !elem.is_object() ) {
                                        ASRTL_ERR_LOG(
                                            "asrtio",
                                            "param config: array element for \"%s\" must be an object",
                                            key.c_str() );
                                        return nullptr;
                                }
                                entries.push_back( &elem );
                        }
                } else {
                        ASRTL_ERR_LOG(
                            "asrtio",
                            "param config: value for \"%s\" must be an object or array",
                            key.c_str() );
                        return nullptr;
                }

                std::vector< asrt::flat_id > roots;
                for ( auto const* entry : entries ) {
                        asrt::flat_id root_id = next_id;
                        if ( !flat_tree_from_json( cfg->tree, *entry, next_id ) ) {
                                ASRTL_ERR_LOG(
                                    "asrtio",
                                    "param config: failed to convert params for \"%s\"",
                                    key.c_str() );
                                return nullptr;
                        }
                        roots.push_back( root_id );
                }

                if ( key == "*" )
                        cfg->wildcard = std::move( roots );
                else
                        cfg->tests[key] = std::move( roots );
        }

        return cfg;
}

std::unique_ptr< param_config > param_config_from_stream( std::istream& in )
{
        nlohmann::json j;
        try {
                j = nlohmann::json::parse( in );
        }
        catch ( nlohmann::json::parse_error const& e ) {
                ASRTL_ERR_LOG( "asrtio", "param config: JSON parse error: %s", e.what() );
                return nullptr;
        }

        return param_config_from_json( j );
}

std::unique_ptr< param_config > param_config_from_file( std::filesystem::path const& path )
{
        std::ifstream file( path );
        if ( !file ) {
                ASRTL_ERR_LOG(
                    "asrtio", "param config: cannot open file \"%s\"", path.string().c_str() );
                return nullptr;
        }

        return param_config_from_stream( file );
}

}  // namespace asrtio
