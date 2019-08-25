/* <base/file_reader.h>

   ----------------------------------------------------------------------------
   Copyright 2017 Dave Peterson <dave@dspeterson.com>

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

     http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
   ----------------------------------------------------------------------------

   File reader convenience class.
 */

#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <ios>
#include <iterator>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <base/no_copy_semantics.h>

namespace Base {

  /* Convenience class for opening a file, reading a bunch of data, and closing
     the file all in a single operation.  On error opening or reading from
     the file, a std::ios_base::failure object is thrown, with its what()
     message set to something useful enough to display to the end user.

     Warning: For methods that store the entire file contents in a std::string
     or std::vector<T>, you should be reasonably certain that the file isn't
     ridiculously large. */
  class TFileReader final {
    NO_COPY_SEMANTICS(TFileReader);

    public:
    explicit TFileReader(const char *filename)
        : Filename(filename) {
      assert(filename);
    }

    /* Return size in bytes of file. */
    size_t GetSize();

    /* Read file contents into caller-supplied buffer with given size.  Read
       entire file into buffer, or as much data as will fit.  Return number of
       bytes written into buffer. */
    size_t ReadIntoBuf(void *buf, size_t buf_size);

    /* Assign to 'dst' the entire contents of the file. */
    void ReadIntoString(std::string &dst);

    /* Return entire file contents. */
    std::string ReadIntoString();

    /* Assign to 'dst' the entire contents of the file. */
    template <typename T>
    void ReadIntoBuf(std::vector<T> &dst) {
      assert(this);
      static_assert(
          std::is_integral<T>::value && (sizeof(T) == sizeof(uint8_t)),
          "Destination vector type parameter must be single-byte integral");
      dst.reserve(GetSize());
      PrepareForRead();

      try {
        dst.assign(std::istreambuf_iterator<T>(Stream),
            std::istreambuf_iterator<T>());
      } catch (const std::ios_base::failure &) {
        ThrowFileReadError();
      }
    }

    /* Return entire file contents. */
    template <typename T>
    std::vector<T> ReadIntoBuf() {
      assert(this);
      std::vector<T> result;
      ReadIntoBuf(result);
      return result;
    }

    private:
    /* On file open error, throw a std::ios_base::failure object with its
       what() message set to something informative.  The system-supplied
       message is practically useless. */
    void ThrowFileOpenError() const;

    /* Same as above for file read error. */
    void ThrowFileReadError() const;

    void Open();

    void PrepareForRead();

    const char * const Filename;

    std::ifstream Stream;
  };  // TFileReader

  inline void ReadFileIntoString(const char *filename, std::string &result) {
    TFileReader(filename).ReadIntoString(result);
  }

  inline void ReadFileIntoString(const std::string &filename,
      std::string &result) {
    ReadFileIntoString(filename.c_str(), result);
  }

  inline std::string ReadFileIntoString(const char *filename) {
    return TFileReader(filename).ReadIntoString();
  }

  inline std::string ReadFileIntoString(const std::string &filename) {
    return ReadFileIntoString(filename.c_str());
  }

  inline size_t ReadFileIntoBuf(const char *filename, void *buf,
      size_t buf_size) {
    return TFileReader(filename).ReadIntoBuf(buf, buf_size);
  }

  inline size_t ReadFileIntoBuf(const std::string &filename, void *buf,
      size_t buf_size) {
    return ReadFileIntoBuf(filename.c_str(), buf, buf_size);
  }

  template <typename T>
  void ReadFileIntoBuf(const char *filename, std::vector<T> &dst) {
    TFileReader(filename).ReadIntoBuf<T>(dst);
  }

  template <typename T>
  void ReadFileIntoBuf(const std::string &filename, std::vector<T> &dst) {
    ReadFileIntoBuf<T>(filename.c_str(), dst);
  }

  template <typename T>
  std::vector<T> ReadFileIntoBuf(const char *filename) {
    return TFileReader(filename).ReadIntoBuf<T>();
  }

  template <typename T>
  std::vector<T> ReadFileIntoBuf(const std::string &filename) {
    return ReadFileIntoBuf<T>(filename.c_str());
  }

}  // Base
