/* <base/to_integer.h>

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

   Utilities for converting strings to integer values.
 */

#pragma once

#include <cassert>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <type_traits>

#include <base/narrow_cast.h>

namespace Base {

  /* Input format specifier. */
  enum class TBase : unsigned int {
    BIN = 1U << 0,  // binary
    OCT = 1U << 1,  // octal
    DEC = 1U << 2,  // decimal
    HEX = 1U << 3  // hexadecimal
  };

  constexpr unsigned int operator&(TBase a, TBase b) noexcept {
    return static_cast<unsigned int>(a) & static_cast<unsigned int>(b);
  }

  constexpr unsigned int operator&(TBase a, unsigned int b) noexcept {
    return static_cast<unsigned int>(a) & b;
  }

  constexpr unsigned int operator&(unsigned int a, TBase b) noexcept {
    return a & static_cast<unsigned int>(b);
  }

  constexpr unsigned int operator|(TBase a, TBase b) noexcept {
    return static_cast<unsigned int>(a) | static_cast<unsigned int>(b);
  }

  constexpr unsigned int operator|(TBase a, unsigned int b) noexcept {
    return static_cast<unsigned int>(a) | b;
  }

  constexpr unsigned int operator|(unsigned int a, TBase b) noexcept {
    return a | static_cast<unsigned int>(b);
  }

  constexpr unsigned int operator!(TBase a) noexcept {
    return !static_cast<unsigned int>(a);
  }

  const unsigned int ALL_BASES =
      TBase::BIN | TBase::OCT | TBase::DEC | TBase::HEX;

  /* Thrown on attempted conversion from string to integer when input is not a
     valid integer. */
  class TInvalidInteger : public std::runtime_error {
    public:
    TInvalidInteger()
        : std::runtime_error("Invalid integer") {
    }
  };

  /* Thrown on attempted conversion from string to integer when input is a
     valid integer, but is not expressed in an allowed base. */
  class TWrongBase : public std::runtime_error {
    public:
    TWrongBase(TBase found, unsigned int allowed);

    TBase GetFound() const noexcept {
      return Found;
    }

    unsigned int GetAllowed() const noexcept {
      return Allowed;
    }

    private:
    TBase Found;

    unsigned int Allowed;
  };

  /* ToSigned<T>() functions below throw the following exceptions:

         - TInvalidInteger if the input is not a valid signed integer in
           decimal format.  Note that a syntactically valid integer that is not
           representable in as many bits as provided by intmax_t is considered
           invalid.

         - std::range_error if the input is valid but the destination type is
           too narrow to represent it (for instance, trying to convert "1000"
           to int8_t).

     ToUnsigned<T>() functions below throw the following exceptions:

         - TInvalidInteger if the input is not a valid unsigned integer in one
           of the following formats:

               binary
                   for instance, 0b1111101001 or 0B0001111101001

               octal
                   for instance, 01751 or 0001751

               decimal
                   for instance, 1001

               hexadecimal
                   for instance, 0x3e9 or 0X00e39

           Note that a syntactically valid integer that is not representable in
           as many bits as provided by uintmax_t is considered invalid.
           Negative numbers are reported as invalid.  In all bases, the value 0
           can be represented as "0".

         - TWrongBase if the input is valid but was specified in a disallowed
           base.

         - std::range_error if the input is valid but the destination type is
           too narrow to represent it (for instance, trying to convert "1000"
           to uint8_t).

     DecimalStringTo<T>(s) functions below behave identically to ToSigned<T>(s)
     if T is signed, or identically to ToUnsigned<T>(s, 0 | TBase::DEC) if T is
     unsigned.

     In all cases above, leading whitespace characters cause TInvalidInteger to
     be thrown.  The entire input string is processed, and any trailing
     characters (including whitespace) that would prevent an otherwise valid
     input from being interpreted as valid cause TInvalidInteger to be thrown.

     Note that for all of the above functions, bool is not a supported type for
     T. */

  /* For converting a string to a signed integral type, only decimal format is
     allowed (i.e. no binary, octal, or hexadecimal). */
  template <typename T>
  T ToSigned(const char *s) {
    static_assert(std::is_signed<T>::value,
        "Type parameter must be a signed integer");

    /* This calls a specialization.  It does not recurse. */
    intmax_t value = ToSigned<intmax_t>(s);

    return narrow_cast<T>(value);
  }

  template <>
  intmax_t ToSigned<intmax_t>(const char *s);

  template <typename T>
  T ToSigned(const std::string &s) {
    return ToSigned<T>(s.c_str());
  }

  /* For converting a string to an unsigned integral type, input may be
     accepted in binary, octal, decimal, hexadecimal, or some nonempty subset
     of these bases.  allowed_bases specifies the allowed bases as a bitwise OR
     of the above-defined TBase values. */
  template <typename T>
  T ToUnsigned(const char *s, unsigned int allowed_bases) {
    static_assert(std::is_unsigned<T>::value && !std::is_same<T, bool>::value,
        "Type parameter must be an unsigned integer other than bool");
    assert(allowed_bases);

    /* This calls a specialization.  It does not recurse. */
    uintmax_t value = ToUnsigned<uintmax_t>(s, allowed_bases);

    return narrow_cast<T>(value);
  }

  template <>
  uintmax_t ToUnsigned<uintmax_t>(const char *s, unsigned int allowed_bases);

  template <typename T>
  T ToUnsigned(const std::string &s, unsigned int allowed_bases) {
    return ToUnsigned<T>(s.c_str(), allowed_bases);
  }

  namespace Impl {  // implementation details

    template <typename T, bool SIGNED = std::is_signed<T>::value>
    struct TDecimalStringToHelper {
      static T DecimalStringTo(const char *s) {
        return ToSigned<T>(s);
      }
    };

    template <typename T>
    struct TDecimalStringToHelper<T, false> {
      static T DecimalStringTo(const char *s) {
        return ToUnsigned<T>(s, 0 | TBase::DEC);
      }
    };

  }

  /* Converts a string to an integral type (either signed or unsigned).  Only
     decimal format is allowed. */
  template <typename T>
  T DecimalStringTo(const char *s) {
    return Impl::TDecimalStringToHelper<T>::DecimalStringTo(s);
  }

  template <typename T>
  T DecimalStringTo(const std::string &s) {
    return DecimalStringTo<T>(s.c_str());
  }

}  // Base
