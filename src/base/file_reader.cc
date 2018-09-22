/* <base/file_reader.cc>

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

   Implements <base/file_reader.h>.
 */

#include <base/file_reader.h>

#include <cerrno>
#include <cstring>

using namespace Base;

size_t Base::TFileReader::GetSize() {
  assert(this);
  Open();
  size_t size = 0;

  try {
    Stream.seekg(0, std::ios::end);
    size = static_cast<size_t>(Stream.tellg());
    Stream.seekg(0, std::ios::beg);
  } catch (const std::ios_base::failure &) {
    ThrowFileReadError();
  }

  return size;
}

size_t Base::TFileReader::ReadIntoBuf(void *buf, size_t buf_size) {
  assert(this);
  PrepareForRead();

  try {
    Stream.read(static_cast<char *>(buf), buf_size);
  } catch (const std::ios_base::failure &) {
    if (!Stream.eof()) {
      ThrowFileReadError();
    }
  }

  auto read_size = static_cast<size_t>(Stream.gcount());
  assert(read_size <= buf_size);
  return read_size;
}

void Base::TFileReader::ReadIntoString(std::string &dst) {
  assert(this);
  dst.reserve(GetSize());
  PrepareForRead();

  try {
    dst.assign(std::istreambuf_iterator<char>(Stream),
        std::istreambuf_iterator<char>());
  } catch (const std::ios_base::failure &) {
    ThrowFileReadError();
  }
}

std::string Base::TFileReader::ReadIntoString() {
  assert(this);
  std::string contents;
  ReadIntoString(contents);
  return contents;
}

void Base::TFileReader::ThrowFileOpenError() const {
  assert(this);
  const char *sys_msg = std::strerror(errno);
  std::string msg("Cannot open file [");
  msg += Filename;
  msg += "] for reading: ";
  msg += sys_msg;
  throw std::ios_base::failure(msg);
}

void Base::TFileReader::ThrowFileReadError() const {
  assert(this);
  const char *sys_msg = std::strerror(errno);
  std::string msg("Cannot read file [");
  msg += Filename;
  msg += "]: ";
  msg += sys_msg;
  throw std::ios_base::failure(msg);
}

void Base::TFileReader::Open() {
  assert(this);

  try {
    /* Clear any errors remaining from a previous operation. */
    Stream.clear();

    if (!Stream.is_open()) {
      Stream.open(Filename, std::ios_base::in | std::ios_base::binary);
      Stream.exceptions(std::ios_base::failbit | std::ios_base::badbit);
    }
  } catch (const std::ios_base::failure &) {
    ThrowFileOpenError();
  }
}

void Base::TFileReader::PrepareForRead() {
  assert(this);
  Open();

  try {
    Stream.seekg(0, std::ios::beg);
  } catch (const std::ios_base::failure &) {
    ThrowFileReadError();
  }
}
