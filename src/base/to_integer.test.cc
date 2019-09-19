/* <base/to_integer.test.cc>

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

   Unit test for <base/to_integer.h>.
 */

#include <base/to_integer.h>

#include <cstdint>
#include <string>

#include <base/error_util.h>

#include <gtest/gtest.h>

using namespace Base;

template <typename T>
void ExpectInvalidSigned(const char *s) {
  bool caught = false;

  try {
    ToSigned<T>(s);
  } catch (const TInvalidInteger &) {
    caught = true;
  } catch (...) {
    ASSERT_TRUE(false);
  }

  ASSERT_TRUE(caught);
  caught = false;

  try {
    ToSigned<T>(std::string(s));
  } catch (const TInvalidInteger &) {
    caught = true;
  } catch (...) {
    ASSERT_TRUE(false);
  }

  ASSERT_TRUE(caught);
  caught = false;

  try {
    DecimalStringTo<T>(s);
  } catch (const TInvalidInteger &) {
    caught = true;
  } catch (...) {
    ASSERT_TRUE(false);
  }

  ASSERT_TRUE(caught);
  caught = false;

  try {
    DecimalStringTo<T>(std::string(s));
  } catch (const TInvalidInteger &) {
    caught = true;
  } catch (...) {
    ASSERT_TRUE(false);
  }

  ASSERT_TRUE(caught);
}

template <typename T>
void ExpectInvalidUnsigned(const char *s) {
  bool caught = false;

  try {
    ToUnsigned<T>(s, TBase::BIN | TBase::OCT | TBase::DEC | TBase::HEX);
  } catch (const TInvalidInteger &) {
    caught = true;
  } catch (...) {
    ASSERT_TRUE(false);
  }

  ASSERT_TRUE(caught);
  caught = false;

  try {
    ToUnsigned<T>(std::string(s),
        TBase::BIN | TBase::OCT | TBase::DEC | TBase::HEX);
  } catch (const TInvalidInteger &) {
    caught = true;
  } catch (...) {
    ASSERT_TRUE(false);
  }

  ASSERT_TRUE(caught);
}

template <typename T>
void ExpectInvalidUnsignedDecimal(const char *s) {
  bool caught = false;

  try {
    ToUnsigned<T>(s, 0 | TBase::DEC);
  } catch (const TInvalidInteger &) {
    caught = true;
  } catch (...) {
    ASSERT_TRUE(false);
  }

  ASSERT_TRUE(caught);
  caught = false;

  try {
    ToUnsigned<T>(std::string(s), 0 | TBase::DEC);
  } catch (const TInvalidInteger &) {
    caught = true;
  } catch (...) {
    ASSERT_TRUE(false);
  }

  ASSERT_TRUE(caught);
  caught = false;

  try {
    DecimalStringTo<T>(s);
  } catch (const TInvalidInteger &) {
    caught = true;
  } catch (...) {
    ASSERT_TRUE(false);
  }

  ASSERT_TRUE(caught);
  caught = false;

  try {
    DecimalStringTo<T>(std::string(s));
  } catch (const TInvalidInteger &) {
    caught = true;
  } catch (...) {
    ASSERT_TRUE(false);
  }

  ASSERT_TRUE(caught);
}

template <typename T>
void ExpectRangeErrorSigned(const char *s) {
  bool caught = false;

  try {
    ToSigned<T>(s);
  } catch (const std::range_error &) {
    caught = true;
  } catch (...) {
    ASSERT_TRUE(false);
  }

  ASSERT_TRUE(caught);
  caught = false;

  try {
    ToSigned<T>(std::string(s));
  } catch (const std::range_error &) {
    caught = true;
  } catch (...) {
    ASSERT_TRUE(false);
  }

  ASSERT_TRUE(caught);
  caught = false;

  try {
    DecimalStringTo<T>(s);
  } catch (const std::range_error &) {
    caught = true;
  } catch (...) {
    ASSERT_TRUE(false);
  }

  ASSERT_TRUE(caught);
  caught = false;

  try {
    DecimalStringTo<T>(std::string(s));
  } catch (const std::range_error &) {
    caught = true;
  } catch (...) {
    ASSERT_TRUE(false);
  }

  ASSERT_TRUE(caught);
}

template <typename T>
void ExpectRangeErrorUnsigned(const char *s) {
  bool caught = false;

  try {
    ToUnsigned<T>(s, TBase::BIN | TBase::OCT | TBase::DEC | TBase::HEX);
  } catch (const std::range_error &) {
    caught = true;
  } catch (...) {
    ASSERT_TRUE(false);
  }

  ASSERT_TRUE(caught);
  caught = false;

  try {
    ToUnsigned<T>(std::string(s),
        TBase::BIN | TBase::OCT | TBase::DEC | TBase::HEX);
  } catch (const std::range_error &) {
    caught = true;
  } catch (...) {
    ASSERT_TRUE(false);
  }

  ASSERT_TRUE(caught);
}

template <typename T>
void ExpectRangeErrorUnsignedDecimal(const char *s) {
  bool caught = false;

  try {
    ToUnsigned<T>(s, 0 | TBase::DEC);
  } catch (const std::range_error &) {
    caught = true;
  } catch (...) {
    ASSERT_TRUE(false);
  }

  ASSERT_TRUE(caught);
  caught = false;

  try {
    ToUnsigned<T>(std::string(s), 0 | TBase::DEC);
  } catch (const std::range_error &) {
    caught = true;
  } catch (...) {
    ASSERT_TRUE(false);
  }

  ASSERT_TRUE(caught);
  caught = false;

  try {
    DecimalStringTo<T>(s);
  } catch (const std::range_error &) {
    caught = true;
  } catch (...) {
    ASSERT_TRUE(false);
  }

  ASSERT_TRUE(caught);
  caught = false;

  try {
    DecimalStringTo<T>(std::string(s));
  } catch (const std::range_error &) {
    caught = true;
  } catch (...) {
    ASSERT_TRUE(false);
  }

  ASSERT_TRUE(caught);
}

template <typename T>
void ExpectWrongBase(const char *s, TBase base, unsigned int allowed_bases) {
  bool caught = false;

  try {
    ToUnsigned<T>(s, allowed_bases);
  } catch (const TWrongBase &x) {
    caught = true;
    ASSERT_EQ(x.GetFound(), base);
    ASSERT_EQ(x.GetAllowed(), allowed_bases);
  } catch (...) {
    ASSERT_TRUE(false);
  }

  ASSERT_TRUE(caught);
  caught = false;

  try {
    ToUnsigned<T>(std::string(s), allowed_bases);
  } catch (const TWrongBase &x) {
    caught = true;
    ASSERT_EQ(x.GetFound(), base);
    ASSERT_EQ(x.GetAllowed(), allowed_bases);
  } catch (...) {
    ASSERT_TRUE(false);
  }

  ASSERT_TRUE(caught);
}

template <typename T>
void ExpectEqualSigned(const char *s, T value) {
  ASSERT_EQ(ToSigned<T>(s), value);
  ASSERT_EQ(ToSigned<T>(std::string(s)), value);
  ASSERT_EQ(DecimalStringTo<T>(s), value);
  ASSERT_EQ(DecimalStringTo<T>(std::string(s)), value);
}

template <typename T>
void ExpectEqualUnsigned(const char *s, T value, unsigned int allowed_bases) {
  ASSERT_EQ(ToUnsigned<T>(s, allowed_bases), value);
  ASSERT_EQ(ToUnsigned<T>(std::string(s), allowed_bases), value);
}

template <typename T>
void ExpectEqualUnsignedDecimal(const char *s, T value) {
  ASSERT_EQ(ToUnsigned<T>(s, 0 | TBase::DEC), value);
  ASSERT_EQ(ToUnsigned<T>(std::string(s), 0 | TBase::DEC), value);
  ASSERT_EQ(DecimalStringTo<T>(s), value);
  ASSERT_EQ(DecimalStringTo<T>(std::string(s)), value);
}

namespace {

  /* The fixture for testing class TBits. */
  class TToIntegerTest : public ::testing::Test {
    protected:
    TToIntegerTest() = default;

    ~TToIntegerTest() override = default;

    void SetUp() override {
    }

    void TearDown() override {
    }
  };  // TToIntegerTest

  TEST_F(TToIntegerTest, Signed) {
    ExpectInvalidSigned<int>("");
    ExpectInvalidSigned<int>("");
    ExpectEqualSigned<int>("0", 0);
    ExpectEqualSigned<int>("1", 1);
    ExpectEqualSigned<int>("-1", -1);
    ExpectInvalidSigned<int>("blahblah");
    ExpectInvalidSigned<int>(" 1");  // reject leading whitespace
    ExpectInvalidSigned<int>("1 ");  // reject trailing whitespace
    ExpectInvalidSigned<int>(" -1");  // reject leading whitespace
    ExpectInvalidSigned<int>("-1 ");  // reject trailing whitespace
    ExpectInvalidSigned<int>("\t1");  // reject leading whitespace
    ExpectInvalidSigned<int>("1\t");  // reject trailing whitespace
    ExpectInvalidSigned<int>("\t-1");  // reject leading whitespace
    ExpectInvalidSigned<int>("-1\t");  // reject trailing whitespace
    ExpectInvalidSigned<int>("1a");  // reject other trailing characters
    ExpectInvalidSigned<int>("-1a");  // reject other trailing characters
    ExpectInvalidSigned<int>("0xa3c");  // unsigned hexadecimal
    ExpectInvalidSigned<int>("0Xa3c");  // unsigned hexadecimal
    ExpectInvalidSigned<int>("0b11010");  // unsigned binary
    ExpectInvalidSigned<int>("0B010010");  // unsigned binary
    ExpectInvalidSigned<int>("0325");  // unsigned octal
    ExpectInvalidSigned<int>("00");  // unsigned octal
    ExpectEqualSigned<int32_t>("8675309", 8675309);
    ExpectEqualSigned<int32_t>("-98765", -98765);
    ExpectEqualSigned<int8_t>("-128", -128);
    ExpectEqualSigned<int8_t>("127", 127);
    ExpectRangeErrorSigned<int8_t>("-129");
    ExpectRangeErrorSigned<int8_t>("128");
    ExpectRangeErrorSigned<int16_t>("32768");
    ExpectRangeErrorSigned<int16_t>("-32769");
    ExpectEqualSigned<int16_t>("32767", 32767);
    ExpectEqualSigned<int16_t>("-32768", -32768);
    ExpectEqualSigned<int32_t>("32768", 32768);
    ExpectEqualSigned<int32_t>("-32769", -32769);
    ExpectInvalidSigned<intmax_t>(
        "9999999999999999999999999999999999999999999999999999999999999999999");
    ExpectInvalidSigned<intmax_t>(
        "-999999999999999999999999999999999999999999999999999999999999999999");
  }

  TEST_F(TToIntegerTest, UnsignedDecimal) {
    ExpectInvalidUnsignedDecimal<unsigned int>("");
    ExpectInvalidUnsignedDecimal<unsigned int>("   ");
    ExpectEqualUnsignedDecimal<unsigned int>("0", 0);
    ExpectEqualUnsignedDecimal<unsigned int>("1", 1);
    ExpectEqualUnsignedDecimal<uint32_t>("8675309", 8675309);

    // reject leading and trailing whitespace
    ExpectInvalidUnsignedDecimal<unsigned int>(" 1");
    ExpectInvalidUnsignedDecimal<unsigned int>(" -1");
    ExpectInvalidUnsignedDecimal<unsigned int>("1 ");
    ExpectInvalidUnsignedDecimal<unsigned int>("\t1");
    ExpectInvalidUnsignedDecimal<unsigned int>("\t-1");
    ExpectInvalidUnsignedDecimal<unsigned int>("1\t");

    // reject other trailing characters
    ExpectInvalidUnsignedDecimal<unsigned int>("1a");

    // reject negative numbers
    ExpectInvalidUnsignedDecimal<unsigned int>("-1");
    ExpectInvalidUnsignedDecimal<unsigned int>("-5");
    ExpectInvalidUnsignedDecimal<uint32_t>("-8675309");
    ExpectInvalidUnsignedDecimal<uintmax_t>(
        "-999999999999999999999999999999999999999999999999999999999999999999");

    ExpectInvalidUnsignedDecimal<unsigned int>("blahblah");
    ExpectInvalidUnsignedDecimal<uintmax_t>(
        "9999999999999999999999999999999999999999999999999999999999999999999");
    ExpectEqualUnsignedDecimal<uint8_t>("255", 255);
    ExpectRangeErrorUnsignedDecimal<uint8_t>("256");
    ExpectEqualUnsignedDecimal<uint16_t>("65535", 65535);
    ExpectRangeErrorUnsignedDecimal<uint16_t>("65536");
    ExpectEqualUnsignedDecimal<uint32_t>("4294967295", 4294967295);
    ExpectRangeErrorUnsignedDecimal<uint32_t>("4294967296");
  }

  TEST_F(TToIntegerTest, UnsignedBases) {
    ExpectEqualUnsigned<unsigned int>("0", 0, ALL_BASES);
    ExpectEqualUnsigned<unsigned int>("1", 1, ALL_BASES);
    ExpectEqualUnsigned<uint32_t>("8675309", 8675309, ALL_BASES);

    // hex value must be preceded by "0x" or "0X"
    ExpectInvalidUnsigned<unsigned int>("1a");
    ExpectEqualUnsigned<unsigned int>("0x1a", 0x1a, ALL_BASES);
    ExpectEqualUnsigned<unsigned int>("0X1a", 0x1a, ALL_BASES);
    ExpectEqualUnsigned<unsigned int>("0x1A", 0x1a, ALL_BASES);
    ExpectEqualUnsigned<unsigned int>("0X01A", 0x1a, ALL_BASES);
    ExpectEqualUnsigned<unsigned int>("0x01a", 0x1a, ALL_BASES);
    ExpectEqualUnsigned<unsigned int>("0X01a", 0x1a, ALL_BASES);
    ExpectEqualUnsigned<unsigned int>("0x01A", 0x1a, ALL_BASES);
    ExpectEqualUnsigned<unsigned int>("0X01A", 0x1a, ALL_BASES);

    ExpectEqualUnsigned<unsigned int>("037", 037, ALL_BASES);
    ExpectEqualUnsigned<unsigned int>("0037", 037, ALL_BASES);
    ExpectInvalidUnsigned<unsigned int>("038");  // invalid octal value

    ExpectEqualUnsigned<unsigned int>("0b10010", 18, ALL_BASES);
    ExpectEqualUnsigned<unsigned int>("0B10010", 18, ALL_BASES);
    ExpectEqualUnsigned<unsigned int>("0b010010", 18, ALL_BASES);
    ExpectEqualUnsigned<unsigned int>("0B010010", 18, ALL_BASES);

    ExpectEqualUnsigned<unsigned int>("0b0", 0, ALL_BASES);
    ExpectEqualUnsigned<unsigned int>("0b1", 1, ALL_BASES);
    ExpectEqualUnsigned<unsigned int>("0b10", 2, ALL_BASES);
    ExpectEqualUnsigned<unsigned int>("0b11", 3, ALL_BASES);
    ExpectEqualUnsigned<unsigned int>("0b100", 4, ALL_BASES);
    ExpectEqualUnsigned<unsigned int>("0b101", 5, ALL_BASES);
    ExpectEqualUnsigned<unsigned int>("0b110", 6, ALL_BASES);
    ExpectEqualUnsigned<unsigned int>("0b111", 7, ALL_BASES);

    ExpectEqualUnsigned<uint8_t>("0377", 0377, ALL_BASES);
    ExpectRangeErrorUnsigned<uint8_t>("0400");
    ExpectEqualUnsigned<uint16_t>("0177777", 0177777, ALL_BASES);
    ExpectRangeErrorUnsigned<uint16_t>("0200000");
    ExpectEqualUnsigned<uint32_t>("037777777777", 037777777777, ALL_BASES);
    ExpectRangeErrorUnsigned<uint32_t>("040000000000");
    ExpectInvalidUnsigned<uintmax_t>(
        "0777777777777777777777777777777777777777777777777777777777777777777");

    ExpectEqualUnsigned<uint8_t>("0xff", 0xff, ALL_BASES);
    ExpectRangeErrorUnsigned<uint8_t>("0x100");
    ExpectEqualUnsigned<uint16_t>("0xffff", 0xffff, ALL_BASES);
    ExpectRangeErrorUnsigned<uint16_t>("0x10000");
    ExpectEqualUnsigned<uint32_t>("0xffffffff", 0xffffffff, ALL_BASES);
    ExpectRangeErrorUnsigned<uint32_t>("0x100000000");
    ExpectInvalidUnsigned<uintmax_t >(
        "0xfffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");

    ExpectEqualUnsigned<uint8_t>("0b11111111", 0xff, ALL_BASES);
    ExpectRangeErrorUnsigned<uint8_t>("0b100000000");
    ExpectEqualUnsigned<uint16_t>("0b1111111111111111", 0xffff, ALL_BASES);
    ExpectRangeErrorUnsigned<uint16_t>("0b10000000000000000");
    ExpectEqualUnsigned<uint32_t>("0b11111111111111111111111111111111",
        0xffffffff, ALL_BASES);
    ExpectRangeErrorUnsigned<uint32_t>("0b100000000000000000000000000000000");
    ExpectInvalidUnsigned<uintmax_t >(
        "0b1111111111111111111111111111111111111111111111111111111111111111111"
        "111111111111111111111111111111111111111111111111111111111111111111111"
        "111111111111111111111111111111111111111111111111111111111111111111111"
        "1111111111111111111111111111111111111111111111111111111111111111111");

    // In all bases, the value 0 can be represented as "0".
    ExpectEqualUnsigned<unsigned int>("0", 0, 0 | TBase::BIN);
    ExpectEqualUnsigned<unsigned int>("0", 0, 0 | TBase::OCT);
    ExpectEqualUnsigned<unsigned int>("0", 0, 0 | TBase::DEC);
    ExpectEqualUnsigned<unsigned int>("0", 0, 0 | TBase::HEX);

    // 0 specified explicitly as octal
    ExpectEqualUnsigned<unsigned int>("00", 0, 0 | TBase::OCT);

    ExpectWrongBase<unsigned int>("0b1010",
        TBase::BIN, TBase::OCT | TBase::DEC | TBase::HEX);
    ExpectWrongBase<unsigned int>("0377",
        TBase::OCT, TBase::BIN | TBase::DEC | TBase::HEX);
    ExpectWrongBase<unsigned int>("00",  // 0 specified explicitly as octal
        TBase::OCT, TBase::BIN | TBase::DEC | TBase::HEX);
    ExpectWrongBase<unsigned int>("123",
        TBase::DEC, TBase::BIN | TBase::OCT | TBase::HEX);
    ExpectWrongBase<unsigned int>("101",
        TBase::DEC, TBase::BIN | TBase::OCT | TBase::HEX);
    ExpectWrongBase<unsigned int>("0xa5",
        TBase::HEX, TBase::BIN | TBase::OCT | TBase::DEC);
    ExpectWrongBase<unsigned int>("0Xa5",
        TBase::HEX, TBase::BIN | TBase::OCT | TBase::DEC);
    ExpectWrongBase<unsigned int>("0x10",
        TBase::HEX, TBase::BIN | TBase::OCT | TBase::DEC);
    ExpectWrongBase<unsigned int>("0X10",
        TBase::HEX, TBase::BIN | TBase::OCT | TBase::DEC);
  }

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  DieOnTerminate();
  return RUN_ALL_TESTS();
}
