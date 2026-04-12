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
#include "./output_fs.hpp"

#include <filesystem>
#include <fstream>
#include <list>

namespace asrtio
{

struct real_fs : output_fs
{
        bool create_directories( std::filesystem::path const& path ) override
        {
                std::error_code ec;
                std::filesystem::create_directories( path, ec );
                return !ec;
        }

        file_writer open_write( std::filesystem::path const& path ) override
        {
                auto& f = streams_.emplace_back();
                f.open( path, std::ios::out | std::ios::trunc );
                return file_writer{ f, *this };
        }

        void close_write( std::ostream& os ) override
        {
                for ( auto it = streams_.begin(); it != streams_.end(); ++it ) {
                        if ( &*it == &os ) {
                                it->close();
                                streams_.erase( it );
                                return;
                        }
                }
        }

private:
        std::list< std::ofstream > streams_;
};

}  // namespace asrtio
