/* <base/hex_dump_writer.cc>

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

   Implements <base/hex_dump_writer.h>.
 */

#include <base/hex_dump_writer.h>

#include <cctype>
#include <cstdio>
#include <iostream>

using namespace Base;

bool THexDumpWriter::DefaultIsPrintableFn(uint8_t value) noexcept {
  return std::isprint(value);
}

void THexDumpWriter::DefaultPrintFn(const std::string &output,
    bool append_newline) {
  std::cout << output;

  if (append_newline) {
    std::cout << std::endl;
  }
}

THexDumpWriter::TFormat::TFormat(size_t bytes_per_line,
    size_t addr_print_width, const std::string &indent,
    const std::string &sep1, const std::string &sep2, const std::string &sep3,
    char nonprintable, const TIsPrintableFn &is_printable_fn)
    : BytesPerLine(bytes_per_line),
      AddrPrintWidth(addr_print_width),
      Indent(indent),
      Sep1(sep1),
      Sep2(sep2),
      Sep3(sep3),
      Nonprintable(nonprintable),
      IsPrintableFn(is_printable_fn) {
}

THexDumpWriter::THexDumpWriter(const TFormat &format)
    : Format(format) {
}

THexDumpWriter::THexDumpWriter(const TPrintFn &print_fn)
    : PrintFn(print_fn) {
}

THexDumpWriter::THexDumpWriter(const TFormat &format, const TPrintFn &print_fn)
    : Format(format),
      PrintFn(print_fn) {
}

/* Append to string 'dst' the result of displaying 'address' as a hexadecimal
   number, with colons separating consecutive 16-bit chunks of the address.
   'width' specifies the number of bytes wide the address should be displayed
   as (must be a value from 1 to 8). */
static void AppendAddrToString(std::string &dst, uint64_t address,
    size_t width) {
  /* force the user's input to be valid */
  if (width < 1) {
    width = 1;
  } else if (width > 8) {
    width = 8;
  }

  /* convert address to string */
  char s[17];
  std::sprintf(s, "%016llx", static_cast<unsigned long long>(address));

  /* write it out, with colons separating consecutive 16-bit chunks of the
     address */
  for (size_t i = 16 - (2 * width); ; ) {
    dst += s[i];

    if (++i >= 16) {
      break;
    }

    if ((i % 4) == 0) {
      dst += ':';
    }
  }
}

/* Append to string 'dst' the two digit hex representation of 'byte'. */
static void AppendHexByteToString(std::string &dst, uint8_t byte) {
  static const char tbl[] = {
    '0', '1', '2', '3', '4', '5', '6', '7',
    '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'
  };

  dst += tbl[byte >> 4];
  dst += tbl[byte & 0x0f];
}

void THexDumpWriter::Write(const void *mem, size_t bytes,
        uint64_t start_addr) const {
  auto *p = static_cast<const uint8_t *>(mem);
  size_t index = 0;
  size_t bytes_left = bytes;
  std::string line;

  /* Each iteration handles one full line of output.  When loop terminates, the
     number of remaining bytes to display (if any) will not be enough to fill
     an entire line. */
  for (;
       bytes_left >= Format.BytesPerLine;
       bytes_left -= Format.BytesPerLine) {
    /* print start address for current line */
    line = Format.Indent;
    AppendAddrToString(line, start_addr + index, Format.AddrPrintWidth);
    line += Format.Sep1;

    /* display the bytes in hex */
    for (size_t i = 0; ; ) {
      AppendHexByteToString(line, p[index++]);

      if (++i >= Format.BytesPerLine) {
        break;
      }

      line += Format.Sep2;
    }

    index -= Format.BytesPerLine;
    line += Format.Sep3;

    /* display the bytes as characters */
    for (size_t i = 0; i < Format.BytesPerLine; ++i) {
      AppendByteCharToString(line, p[index++]);
    }

    PrintFn(line, true);
  }

  if (bytes_left == 0) {
    return;
  }

  /* print start address for last line */
  line = Format.Indent;
  AppendAddrToString(line, start_addr + index, Format.AddrPrintWidth);
  line += Format.Sep1;

  size_t i = 0;

  /* display bytes for last line in hex */
  for (; i < bytes_left; ++i) {
    AppendHexByteToString(line, p[index++]);
    line += Format.Sep2;
  }

  index -= bytes_left;

  /* pad the rest of the hex byte area with spaces */
  for (; ; ) {
    line += "  ";

    if (++i >= Format.BytesPerLine) {
      break;
    }

    line += Format.Sep2;
  }

  line += Format.Sep3;

  /* display bytes for last line as characters */
  for (i = 0; i < bytes_left; ++i) {
    AppendByteCharToString(line, p[index++]);
  }

  /* pad the rest of the character area with spaces */
  for (; i < Format.BytesPerLine; ++i) {
    line += ' ';
  }

  PrintFn(line, true);
}

/* Append to string 'dst' the character representation of 'byte'. */
void THexDumpWriter::AppendByteCharToString(std::string &dst,
    uint8_t byte) const {
  dst += Format.IsPrintableFn(byte) ?
      static_cast<char>(byte) : Format.Nonprintable;
}
