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
#include <filesystem>
#include <ostream>
#include <utility>

namespace asrtio
{

struct output_fs;

/// Thin RAII wrapper over std::ostream&.
/// Calls output_fs::close_write() on destruction.
struct file_writer
{
        file_writer( std::ostream& os, output_fs& fs )
          : _os( &os )
          , _fs( &fs )
        {
        }

        file_writer( file_writer&& o ) noexcept
          : _os( std::exchange( o._os, nullptr ) )
          , _fs( std::exchange( o._fs, nullptr ) )
        {
        }

        file_writer& operator=( file_writer&& ) = delete;
        file_writer( file_writer const& )       = delete;

        std::ostream& stream() { return *_os; }

        ~file_writer();

private:
        std::ostream* _os;
        output_fs*    _fs;
};

struct output_fs
{
        /// Create directory and all parents.  Returns true on success
        /// (or if the directory already exists).
        virtual bool create_directories( std::filesystem::path const& path ) = 0;

        /// Open a file for writing (create or truncate).
        /// Returns a file_writer whose stream() can be used to write data.
        /// The file is closed when the file_writer is destroyed.
        virtual file_writer open_write( std::filesystem::path const& path ) = 0;

        /// Called by file_writer destructor — finalises the write.
        virtual void close_write( std::ostream& os ) = 0;

        virtual ~output_fs() = default;
};

inline file_writer::~file_writer()
{
        if ( _fs )
                _fs->close_write( *_os );
}

/// No-op output_fs — all operations succeed but produce no output.
struct null_fs : output_fs
{
        bool create_directories( std::filesystem::path const& ) override { return true; }

        file_writer open_write( std::filesystem::path const& ) override
        {
                return file_writer{ _sink, *this };
        }

        void close_write( std::ostream& ) override {}

private:
        struct null_buf : std::streambuf
        {
        protected:
                int_type overflow( int_type c ) override { return c; }
        };

        null_buf     _buf;
        std::ostream _sink{ &_buf };
};

}  // namespace asrtio
