/* <base/hex_dump_writer.h>

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

   Utility for displaying a hex dump of a sequence of bytes.  Useful for
   debugging.
 */

#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <utility>

namespace Base {

  class THexDumpWriter final {
    public:
    /* Function that takes a byte as input, and returns true if it is
       printable, or false otherwise.  If a byte is nonprintable, a substitute
       character will be printed instead. */
    using TIsPrintableFn = std::function<bool(uint8_t) noexcept>;

    /* A client-supplied function for printing a string.  The first parameter
       is the string to print.  A true value for the second parameter indicates
       that a newline should be appended to the output.  The THexDumpWriter
       implementation calls this function as needed to print the hex dump.  The
       function may assume that all characters in the string to be printed are
       printable. */
    using TPrintFn = std::function<void(const std::string &, bool)>;

    /* Default "is printable" function if none is supplied by client.  Returns
       the same result as std::isprint(value). */
    static bool DefaultIsPrintableFn(uint8_t value) noexcept;

    /* Default print function if none is supplied by client.  This writes to
       standard output. */
    static void DefaultPrintFn(const std::string &output, bool append_newline);

    /* defines how output is formatted */
    class TFormat final {
      public:
      /* constructor specifies a default format */
      TFormat() = default;

      /* constructor allows client to specify all format parameters */
      TFormat(size_t bytes_per_line, size_t addr_print_width,
          const std::string &indent, const std::string &sep1,
          const std::string &sep2, const std::string &sep3, char nonprintable,
          const TIsPrintableFn &is_printable_fn);

      size_t GetBytesPerLine() const noexcept {
        assert(this);
        return BytesPerLine;
      }

      TFormat &SetBytesPerLine(size_t bytes_per_line) noexcept {
        assert(this);
        BytesPerLine = bytes_per_line;
        return *this;
      }

      size_t GetAddrPrintWidth() const noexcept {
        assert(this);
        return AddrPrintWidth;
      }

      TFormat &SetAddrPrintWidth(size_t addr_print_width) noexcept {
        assert(this);
        AddrPrintWidth = addr_print_width;
        return *this;
      }

      const std::string &GetIndent() const noexcept {
        assert(this);
        return Indent;
      }

      TFormat &SetIndent(const std::string &indent) {
        assert(this);
        Indent = indent;
        return *this;
      }

      TFormat &SetIndent(std::string &&indent) noexcept {
        assert(this);
        Indent = std::move(indent);
        return *this;
      }

      const std::string &GetSep1() const noexcept {
        assert(this);
        return Sep1;
      }

      TFormat &SetSep1(const std::string &sep1) {
        assert(this);
        Sep1 = sep1;
        return *this;
      }

      TFormat &SetSep1(std::string &&sep1) noexcept {
        assert(this);
        Sep1 = std::move(sep1);
        return *this;
      }

      const std::string &GetSep2() const noexcept {
        assert(this);
        return Sep2;
      }

      TFormat &SetSep2(const std::string &sep2) {
        assert(this);
        Sep2 = sep2;
        return *this;
      }

      TFormat &SetSep2(std::string &&sep2) noexcept {
        assert(this);
        Sep2 = std::move(sep2);
        return *this;
      }

      const std::string &GetSep3() const noexcept {
        assert(this);
        return Sep3;
      }

      TFormat &SetSep3(const std::string &sep3) {
        assert(this);
        Sep3 = sep3;
        return *this;
      }

      TFormat &SetSep3(std::string &&sep3) noexcept {
        assert(this);
        Sep3 = std::move(sep3);
        return *this;
      }

      char GetNonprintable() const noexcept {
        assert(this);
        return Nonprintable;
      }

      TFormat &SetNonprintable(char nonprintable) noexcept {
        assert(this);
        Nonprintable = nonprintable;
        return *this;
      }

      const TIsPrintableFn &GetIsPrintableFn() const noexcept {
        assert(this);
        return IsPrintableFn;
      }

      TFormat &SetIsPrintableFn(
          const TIsPrintableFn &is_printable_fn) {
        assert(this);
        IsPrintableFn = is_printable_fn;
        return *this;
      }

      TFormat &SetIsPrintableFn(TIsPrintableFn &&is_printable_fn) {
        assert(this);
        IsPrintableFn = std::move(is_printable_fn);
        return *this;
      }

      private:
      /* This is the number of data bytes to display per line of output. */
      size_t BytesPerLine = 8;

      /* Each line of output begins with the address of the first data byte
         displayed on that line.  This specifies the number of bytes wide the
         address should be displayed as.  The value must be from 1 to 8. */
      size_t AddrPrintWidth = 8;

      /* This is a string to display at the start of each output line.  Its
         purpose is to indent the output. */
      std::string Indent = "";

      /* This is a string to display between the address and the bytes of data
         displayed in hex.  It serves as a separator. */
      std::string Sep1 = " | ";

      /* This is a string to display between individual hex values.  It serves
         as a separator. */
      std::string Sep2 = " ";

      /* This is a string to display between the bytes of data in hex and the
         bytes of data displayed as characters.  It serves as a separator. */
      std::string Sep3 = " | ";

      /* This is a substitute character to display in place of nonprintable
         characters. */
      char Nonprintable = '.';

      /* This is a client-supplied function that takes a byte value as input
         and returns a boolean value indicating whether the corresponding
         character is printable. */
      TIsPrintableFn IsPrintableFn = DefaultIsPrintableFn;

      friend class THexDumpWriter;  // for easy access
    };  // TFormat

    /* This constructor specifies a default-constructed TFormat object, and
       DefaultPrintFn() as the print function. */
    THexDumpWriter() = default;

    /* This constructor allows the client to specify a TFormat object, and uses
       DefaultPrintFn() as the print function. */
    explicit THexDumpWriter(const TFormat &format);

    /* This constructor allows the client to specify a print function, and
       specifies a default-constructed TFormat object. */
    explicit THexDumpWriter(const TPrintFn &print_fn);

    /* This constructor allows the client to specify both the format and the
       print function. */
    THexDumpWriter(const TFormat &format, const TPrintFn &print_fn);

    /* Print a hex dump of the memory region pointed to by 'mem', where 'bytes'
       specifies the size of the region in bytes.  'start_addr' specifies the
       address to associate with the first byte of data.  For instance, a value
       of 0 indicates that the first byte displayed should be labeled as byte
       0. */
    void Write(const void *mem, size_t bytes, uint64_t start_addr = 0) const;

    private:
    void AppendByteCharToString(std::string &dst, uint8_t byte) const;

    TFormat Format;

    TPrintFn PrintFn = DefaultPrintFn;
  };  // THexDumpWriter

}  // Base
