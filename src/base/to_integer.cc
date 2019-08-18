/* <base/to_integer.cc>

   ----------------------------------------------------------------------------
   Copyright 2019 Dave Peterson <dave@dspeterson.com>

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

   Implements <base/to_integer.h>.
 */

#include <base/to_integer.h>

#include <cctype>
#include <cstddef>
#include <ios>
#include <sstream>

#include <base/bits.h>
#include <base/no_default_case.h>

using namespace Base;

static std::string MakeWrongBaseMsg(TBase found, unsigned int allowed) {
  assert(allowed);
  std::ostringstream os;
  os << "Integer specidied in wrong base (";

  switch (found) {
    case TBase::BIN: {
      os << "binary";
      break;
    }
    case TBase::OCT: {
      os << "octal";
      break;
    }
    case TBase::DEC: {
      os << "decimal";
      break;
    }
    case TBase::HEX: {
      os << "hexadecimal";
      break;
    }
    NO_DEFAULT_CASE;
  }

  os << ").  Allowed bases: ";
  bool following = false;

  if (allowed & TBase::BIN) {
    os << "binary";
    following = true;
  }

  if (allowed & TBase::OCT) {
    if (following) {
      os << ", ";
    }

    os << "octal";
    following = true;
  }

  if (allowed & TBase::DEC) {
    if (following) {
      os << ", ";
    }

    os << "decimal";
    following = true;
  }

  if (allowed & TBase::HEX) {
    if (following) {
      os << ", ";
    }

    os << "hexadecimal";
    following = true;
  }

  return os.str();
}

TWrongBase::TWrongBase(TBase found, unsigned int allowed)
    : std::runtime_error(MakeWrongBaseMsg(found, allowed)),
      Found(found),
      Allowed(allowed) {
}

static uintmax_t BinaryDigitStringToInteger(const char *s,
    unsigned int allowed_bases) {
  assert(s);

  if (*s == '\0') {
    throw TInvalidInteger();
  }

  // Skip leading zeroes.
  while (*s == '0') {
    ++s;
  }

  uintmax_t result = 0;
  size_t bits = 0;
  const size_t max_bits = TBits<uintmax_t>::Value();

  for (; ; ) {
    const char digit = *s;

    if (digit == '\0') {
      break;
    }

    assert(bits <= max_bits);

    if (bits == max_bits) {
      throw TInvalidInteger();  // too many bits
    }

    result <<= 1;

    if (digit == '1') {
      ++result;
    } else if (digit != '0') {
      throw TInvalidInteger();
    }

    ++bits;
    ++s;
  }

  if ((allowed_bases & TBase::BIN) == 0) {
    /* A valid unsigned integer was found, but binary is not allowed. */
    throw TWrongBase(TBase::BIN, allowed_bases);
  }

  return result;
}

template <>
intmax_t Base::ToSigned<intmax_t>(const char *s) {
  assert(s);

  if (std::isspace(*s)) {
    // Reject leading whitespace here, since the conversion below using
    // std::stringstream will accept it.
    throw TInvalidInteger();
  }

  // We view anything other than "0" that starts with '0' as an octal unsigned
  // value, which we treat as an invalid signed integer.
  if ((s[0] == '0') && s[1] != '\0') {
    throw TInvalidInteger();
  }

  std::stringstream ss;
  intmax_t result = 0;
  ss << s;
  ss >> result;

  if (ss.fail() || (ss.peek() != std::stringstream::traits_type::eof())) {
    throw TInvalidInteger();
  }

  return result;
}

template <>
uintmax_t Base::ToUnsigned(const char *s, unsigned int allowed_bases) {
  assert(s);
  assert(allowed_bases);

  if (std::isspace(*s)) {
    // Reject leading whitespace here, since the conversion below using
    // std::stringstream will accept it.
    throw TInvalidInteger();
  }

  uintmax_t result = 0;
  TBase base = TBase::DEC;
  std::stringstream ss;

  switch (s[0]) {
    case '-': {
      // Reject negative numbers here, since the conversion below using
      // std::stringstream will accept them.
      throw TInvalidInteger();
    }
    case '0': {
      switch (s[1]) {
        case '\0': {
          return result;
        }
        case 'b':
        case 'B': {
          return BinaryDigitStringToInteger(&s[2], allowed_bases);
        }
        case 'x':
        case 'X': {
          base = TBase::HEX;
          ss << std::hex;
          break;
        }
        default: {
          base = TBase::OCT;
          ss << std::oct;
          break;
        }
      }
    }
    default: {
      break;
    }
  }

  ss << s;
  ss >> result;

  if (ss.fail() || (ss.peek() != std::stringstream::traits_type::eof())) {
    throw TInvalidInteger();
  }

  if ((allowed_bases & base) == 0) {
    /* A valid unsigned integer was found, but the format is not allowed. */
    throw TWrongBase(base, allowed_bases);
  }

  return result;
}
