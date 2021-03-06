// Copyright 2012 Matthew Nourse and n plus 1 computing pty limited unless otherwise noted.
// Please see LICENSE file for details.
#ifndef NP1_IO_SNAPPY_FILE_HPP
#define NP1_IO_SNAPPY_FILE_HPP



#include "np1/io/file.hpp"
#include "snappy.h"



namespace np1 {
namespace io {

/// A class for files compressed with Google's 'snappy' compressor.  Also reads uncompressed files automatically.
class snappy_file : public detail::stream_helper<snappy_file> {
public:
  /// Default constructor.
  gzfile() : detail::stream_helper<gzfile>(*this), m_gzhandle(0) {}

  /// Automatically close if not a std handle.
  ~gzfile() {
    close();        
  }

  /// Returns false on error.
  bool create_wo(const char *file_name, int compression_level = DEFAULT_ZLIB_COMPRESSION_LEVEL) {
    return open(file_name, false, compression_level);
  }


  /// Returns false on error.
  bool open_ro(const char *file_name) {
    return open(file_name, true, DEFAULT_ZLIB_COMPRESSION_LEVEL);
  }

  
  /// Is the file open?
  bool is_open() const { return m_file.is_open(); }


  /**
   * Returns false and sets *bytes_read_p=0 on error.  Returns true
   * and sets *bytes_read_p=0 on EOF.
   */
  bool read_some(void *buf, size_t bytes_to_read, size_t *bytes_read_p) {
    if (bytes_to_read > INT_MAX) {
      bytes_to_read = INT_MAX - 1;
    }

    int bytes_read = gzread(m_gzhandle, buf, bytes_to_read);
    if (bytes_read < 0) {
      return false;
    }

    *bytes_read_p = bytes_read;
    return true;
  }
 

  /// Returns false and sets *bytes_written_p=0 on error. 
  bool write_some(const void *buf, size_t bytes_to_write, size_t *bytes_written_p) {
    if (bytes_to_write > INT_MAX) {
      bytes_to_write = INT_MAX - 1;
    }

    int bytes_written = gzwrite(m_gzhandle, buf, bytes_to_write);
    if (bytes_written <= 0) {
      return false;
    }

    *bytes_written_p = bytes_written;
    return true;
  }


  /// Go back to the start of the stream.
  bool rewind() {
    return (::gzrewind(m_gzhandle) == 0);
  }


  /// Soft flush- use sparingly as it degrades compression performance.
  bool soft_flush() {
    return (::gzflush(m_gzhandle, 0) == Z_OK);
  }

  bool lock_exclusive() {
    return m_file.lock_exclusive();
  }

  bool unlock() {
    return m_file.unlock();
  }


  /// Close the file handle, noop if file handle is invalid or a std handle.
  bool close() {
    if (!m_gzhandle) {
      return true;
    }

    soft_flush();
    int int_result = ::gzclose(m_gzhandle);
    bool result = (Z_OK == int_result);
    if (!result) {
      const char *reason;
      switch (int_result) {
      case Z_STREAM_ERROR:
        reason = "Stream is not valid";
        break;

      case Z_ERRNO:
        reason = "A file error occurred";
        break;

      default:
        reason = "Strange things are afoot at the circle K";
        break;
      }

      fprintf(stderr, "Unable to close gzfile because: %s\n", reason);
    }

    m_file.release();
    m_gzhandle = 0;
    return result;
  }

  const rstd::string &name() const {
    return m_file.name();
  }


private:
  /// Disable copy.
  gzfile(const gzfile &);
  gzfile &operator = (const gzfile &);

private:
  bool open(const char *file_name, bool is_read, int compression_level) {
    close();

    bool result = is_read ? m_file.open_ro(file_name) : m_file.create_wo(file_name); 
    if (!result) {
      return false;    
    }

    int fd = m_file.handle();
    m_gzhandle = ::gzdopen(fd, is_read ? "rb" : ("wb" + str::to_dec_str(compression_level)).c_str());
    if (!m_gzhandle) {
      m_file.close();
      return false;
    }

    size_t buffer_size = DEFAULT_ZLIB_IO_BUFFER_SIZE;
    uint64_t file_size = 0;    
    // /2 because zlib doubles the read buffer size.
    uint64_t file_size_based_buffer_size = file::get_size(file_name, file_size) ? file_size/2 : 0;     

    if (is_read && (file_size_based_buffer_size > buffer_size)) {
      buffer_size =
        (file_size_based_buffer_size < MAX_ZLIB_IO_BUFFER_SIZE) ? file_size_based_buffer_size : MAX_ZLIB_IO_BUFFER_SIZE;
    }

    if (::gzbuffer(m_gzhandle, buffer_size) != 0) {
       ::gzclose(m_gzhandle);
       m_file.release();
       return false;      
    }

    return true;
  }


private:
  gzFile m_gzhandle;
  file m_file;
};


} // namespaces
}

#endif
