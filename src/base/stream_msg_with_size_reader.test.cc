/* <base/stream_msg_with_size_reader.test.cc>
 
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
 
   Unit test for <base/stream_msg_with_size_reader.h>.
 */

#include <base/stream_msg_with_size_reader.h>

#include <cassert>
#include <cstdint>
#include <stdexcept>

#include <fcntl.h>
#include <unistd.h>

#include <base/error_util.h>
#include <base/field_access.h>
#include <base/wr/fd_util.h>

#include <gtest/gtest.h>

using namespace Base;
 
namespace {

  struct TPipe {
    int Read;

    int Write;

    TPipe(int read_end, int write_end)
        : Read(read_end),
          Write(write_end) {
    }

    void CloseRead() {
      assert(this);

      if (Read >= 0) {
        Wr::close(Read);
        Read = -1;
      }
    }

    void CloseWrite() {
      assert(this);

      if (Write >= 0) {
        Wr::close(Write);
        Write = -1;
      }
    }

    ~TPipe() {
      CloseRead();
      CloseWrite();
    }
  };  // TPipe

  TPipe MakePipe(bool nonblocking = false) {
    int pipefd[2];
    Wr::pipe2(pipefd, nonblocking ? O_NONBLOCK : 0);
    return TPipe(pipefd[0], pipefd[1]);
  }

  void WritePipe(int pipefd, const std::string &s) {
    ssize_t bytes = Wr::write(Wr::TDisp::Nonfatal, {}, pipefd, s.data(),
        s.size());

    if (static_cast<size_t>(bytes) != s.size()) {
      /* The amounts of data written in these tests will be small enough that
         the entire write should always complete.  Therefore we should never
         get here.  Just in case we do, throw an exception because if the
         assumption that all bytes are written is ever violated, tests will
         fail. */
      Die("Short pipe write in test for TStreamMsgWithSizeReader");
    }
  }

  void WritePipe(int pipefd, const void *data, size_t data_size) {
    ssize_t bytes = Wr::write(Wr::TDisp::Nonfatal, {}, pipefd, data,
        data_size);

    if (static_cast<size_t>(bytes) != data_size) {
      /* The amounts of data written in these tests will be small enough that
         the entire write should always complete.  Therefore we should never
         get here.  Just in case we do, throw an exception because if the
         assumption that all bytes are written is ever violated, tests will
         fail. */
      Die("Short pipe write in test for TStreamMsgWithSizeReader");
    }
  }

  std::string MakeReadyMsgStr(const TStreamMsgWithSizeReaderBase &r) {
    size_t msg_size = r.GetReadyMsgSize();
    const uint8_t *msg = r.GetReadyMsg();

    /* If the reader was configured to include the size field in the message,
       remove the size field from the returned result for the purpose of these
       tests. */
    if (r.GetIncludeSizeFieldInMsg()) {
      size_t size_field_size = r.GetSizeFieldSize();
      assert(msg_size >= size_field_size);
      msg_size -= size_field_size;
      msg += size_field_size;
    }

    if (msg_size == 0) {
      return "";
    }

    return std::string(reinterpret_cast<const char *>(msg), msg_size);
  }

  std::string MakeDataStr(const TStreamMsgReader &r, size_t offset) {
    if (offset > r.GetDataSize()) {
      Die("Offset out of range in MakeDataStr()");
    }

    if (r.GetDataSize() == offset) {
      return "";
    }

    return std::string(reinterpret_cast<const char *>(r.GetData()) + offset,
        r.GetDataSize() - offset);
  }

  /* The fixture for testing class TStreamMsgWithSizeReader. */
  class TStreamMsgWithSizeReaderTest : public ::testing::Test {
    protected:
    TStreamMsgWithSizeReaderTest() {
    }

    virtual ~TStreamMsgWithSizeReaderTest() {
    }

    virtual void SetUp() {
    }

    virtual void TearDown() {
    }
  };  // TStreamMsgWithSizeReaderTest


  TEST_F(TStreamMsgWithSizeReaderTest, Test1) {
    TPipe p(MakePipe());
    TStreamMsgWithSizeReader<int32_t> r(p.Read, false, false, 32, 64);
    ASSERT_EQ(r.GetState(), TStreamMsgReader::TState::ReadNeeded);
    ASSERT_EQ(r.GetFd(), p.Read);

    /* write first 2 bytes of 4 byte size field to pipe */
    int32_t size_field = 0;
    WriteInt32ToHeader(&size_field, 10);
    WritePipe(p.Write, &size_field, 2);

    auto state = r.Read();
    ASSERT_EQ(state, TStreamMsgReader::TState::ReadNeeded);

    /* write last 2 bytes of 4 byte size field to pipe */
    WritePipe(p.Write, reinterpret_cast<const uint8_t *>(&size_field) + 2, 2);

    state = r.Read();
    ASSERT_EQ(state, TStreamMsgReader::TState::ReadNeeded);

    /* write first 3 bytes of 10 byte message body */
    WritePipe(p.Write, "123");

    state = r.Read();
    ASSERT_EQ(state, TStreamMsgReader::TState::ReadNeeded);

    /* write next 6 bytes of 10 byte message body */
    WritePipe(p.Write, "456789");

    state = r.Read();
    ASSERT_EQ(state, TStreamMsgReader::TState::ReadNeeded);

    /* write last byte of 10 byte message body */
    WritePipe(p.Write, "0");

    state = r.Read();
    ASSERT_EQ(state, TStreamMsgReader::TState::MsgReady);

    /* verify that ready message has correct contents */
    ASSERT_EQ(MakeReadyMsgStr(r), "1234567890");

    /* make sure data looks reasonable */
    ASSERT_EQ(MakeDataStr(r, 4), "1234567890");

    state = r.ConsumeReadyMsg();
    ASSERT_EQ(state, TStreamMsgReader::TState::ReadNeeded);

    /* data should be consumed */
    ASSERT_EQ(MakeDataStr(r, 0), "");

    /* write size field with value of 5 */
    WriteInt32ToHeader(&size_field, 5);
    WritePipe(p.Write, &size_field, 4);

    /* write first 2 bytes of 5 byte message body */
    WritePipe(p.Write, "aa");

    state = r.Read();
    ASSERT_EQ(state, TStreamMsgReader::TState::ReadNeeded);

    /* write last 3 bytes of 5 byte message body */
    WritePipe(p.Write, "aaa");

    /* write size field with value of 3 */
    WriteInt32ToHeader(&size_field, 3);
    WritePipe(p.Write, &size_field, 4);

    /* write first byte of 3 byte message body */
    WritePipe(p.Write, "b");

    state = r.Read();
    ASSERT_EQ(state, TStreamMsgReader::TState::MsgReady);

    /* verify that ready message has correct contents */
    ASSERT_EQ(MakeReadyMsgStr(r), "aaaaa");

    /* sanity check data size */
    ASSERT_EQ(r.GetDataSize(), 14U);

    state = r.ConsumeReadyMsg();
    ASSERT_EQ(state, TStreamMsgReader::TState::ReadNeeded);

    /* sanity check data size */
    ASSERT_EQ(r.GetDataSize(), 5U);

    /* write last 2 bytes of 3 byte message body */
    WritePipe(p.Write, "bb");

    /* write size field with value of 4 */
    WriteInt32ToHeader(&size_field, 4);
    WritePipe(p.Write, &size_field, 4);

    /* write 4 byte message body */
    WritePipe(p.Write, "cccc");

    /* write size field with value of 5 */
    WriteInt32ToHeader(&size_field, 5);
    WritePipe(p.Write, &size_field, 4);

    /* write 5 byte message body */
    WritePipe(p.Write, "ddddd");

    state = r.Read();
    ASSERT_EQ(state, TStreamMsgReader::TState::MsgReady);

    /* verify that ready message has correct contents */
    ASSERT_EQ(MakeReadyMsgStr(r), "bbb");

    state = r.ConsumeReadyMsg();
    ASSERT_EQ(state, TStreamMsgReader::TState::MsgReady);

    /* verify that ready message has correct contents */
    ASSERT_EQ(MakeReadyMsgStr(r), "cccc");

    state = r.ConsumeReadyMsg();
    ASSERT_EQ(state, TStreamMsgReader::TState::MsgReady);

    /* verify that ready message has correct contents */
    ASSERT_EQ(MakeReadyMsgStr(r), "ddddd");

    /* all done writing data */
    p.CloseWrite();

    /* mark last message as consumed */
    state = r.ConsumeReadyMsg();
    ASSERT_EQ(state, TStreamMsgReader::TState::ReadNeeded);

    /* try to read more, and verify that reader has reached end */
    state = r.Read();
    ASSERT_EQ(state, TStreamMsgReader::TState::AtEnd);

    /* sanity check data size */
    ASSERT_EQ(r.GetDataSize(), 0U);
  }

  TEST_F(TStreamMsgWithSizeReaderTest, Test2) {
    TPipe p(MakePipe());

    /* preferred read size is 32 bytes; this time try with
       size_includes_size_field set to true */
    TStreamMsgWithSizeReader<uint16_t> r(p.Read, true, false, 64, 32);

    /* write size field with value of 22 */
    uint16_t size_field = 0;
    WriteUint16ToHeader(&size_field, 22);
    WritePipe(p.Write, &size_field, 2);

    /* write entire message body */
    WritePipe(p.Write, "twenty bytes message");

    /* write size field with value of 12 */
    WriteUint16ToHeader(&size_field, 12);
    WritePipe(p.Write, &size_field, 2);

    /* write entire message body */
    WritePipe(p.Write, "1234567890");
    p.CloseWrite();

    ASSERT_EQ(r.GetState(), TStreamMsgReader::TState::ReadNeeded);
    auto state = r.Read();
    ASSERT_EQ(state, TStreamMsgReader::TState::MsgReady);

    /* data size should equal preferred read size */
    ASSERT_EQ(r.GetDataSize(), 32U);

    /* verify that ready message has correct contents */
    ASSERT_EQ(MakeReadyMsgStr(r), "twenty bytes message");

    /* mark message consumed; 10 bytes of data should be buffered, with 2 more
       bytes in pipe */
    state = r.ConsumeReadyMsg();
    ASSERT_EQ(state, TStreamMsgReader::TState::ReadNeeded);
    ASSERT_EQ(r.GetDataSize(), 10U);

    state = r.Read();
    ASSERT_EQ(state, TStreamMsgReader::TState::MsgReady);

    /* data size should be 12 because we started with 10 bytes and should have
       read 2 more bytes */
    ASSERT_EQ(r.GetDataSize(), 12U);

    /* verify that ready message has correct contents */
    ASSERT_EQ(MakeReadyMsgStr(r), "1234567890");

    /* mark message consumed; 0 bytes of data should be buffered */
    state = r.ConsumeReadyMsg();
    ASSERT_EQ(state, TStreamMsgReader::TState::ReadNeeded);
    ASSERT_EQ(r.GetDataSize(), 0U);

    /* try to read more, and verify that reader has reached end */
    state = r.Read();
    ASSERT_EQ(state, TStreamMsgReader::TState::AtEnd);

    /* sanity check data size */
    ASSERT_EQ(r.GetDataSize(), 0U);
  }

  TEST_F(TStreamMsgWithSizeReaderTest, Test3) {
    TPipe p(MakePipe());

    /* max message body size is 10 bytes */
    TStreamMsgWithSizeReader<int64_t> r(p.Read, true, false, 10, 32);

    /* write size field with value of 18 */
    int64_t size_field = 0;
    WriteInt64ToHeader(&size_field, 18);
    WritePipe(p.Write, &size_field, 8);

    /* write 10 byte message body */
    WritePipe(p.Write, "1234567890");

    ASSERT_EQ(r.GetState(), TStreamMsgReader::TState::ReadNeeded);

    auto state = r.Read();
    ASSERT_EQ(state, TStreamMsgReader::TState::MsgReady);

    /* verify that ready message has correct contents */
    ASSERT_EQ(MakeReadyMsgStr(r), "1234567890");

    state = r.ConsumeReadyMsg();
    ASSERT_EQ(state, TStreamMsgReader::TState::ReadNeeded);

    /* write size field with value of 19 */
    WriteInt64ToHeader(&size_field, 19);
    WritePipe(p.Write, &size_field, 8);

    /* write 11 byte message body which is > max allowed size */
    WritePipe(p.Write, "1234567890a");

    state = r.Read();
    ASSERT_EQ(state, TStreamMsgReader::TState::DataInvalid);
    ASSERT_TRUE(r.GetDataInvalidReason().IsKnown());
    ASSERT_EQ(*r.GetDataInvalidReason(),
        TStreamMsgWithSizeReaderBase::TDataInvalidReason::MsgBodyTooLarge);
  }

  TEST_F(TStreamMsgWithSizeReaderTest, Test4) {
    TPipe p(MakePipe());

    /* max message body size is 10 bytes; this is same as previous test except
       that size_includes_size_field is false */
    TStreamMsgWithSizeReader<int64_t> r(p.Read, false, false, 10, 32);

    /* write size field with value of 10 */
    int64_t size_field = 0;
    WriteInt64ToHeader(&size_field, 10);
    WritePipe(p.Write, &size_field, 8);

    /* write 10 byte message body */
    WritePipe(p.Write, "1234567890");

    ASSERT_EQ(r.GetState(), TStreamMsgReader::TState::ReadNeeded);

    auto state = r.Read();
    ASSERT_EQ(state, TStreamMsgReader::TState::MsgReady);

    /* verify that ready message has correct contents */
    ASSERT_EQ(MakeReadyMsgStr(r), "1234567890");

    state = r.ConsumeReadyMsg();
    ASSERT_EQ(state, TStreamMsgReader::TState::ReadNeeded);

    /* write size field with value of 11 */
    WriteInt64ToHeader(&size_field, 11);
    WritePipe(p.Write, &size_field, 8);

    /* write 11 byte message body which is > max allowed size */
    WritePipe(p.Write, "1234567890a");

    state = r.Read();
    ASSERT_EQ(state, TStreamMsgReader::TState::DataInvalid);
    ASSERT_TRUE(r.GetDataInvalidReason().IsKnown());
    ASSERT_EQ(*r.GetDataInvalidReason(),
        TStreamMsgWithSizeReaderBase::TDataInvalidReason::MsgBodyTooLarge);
  }

  TEST_F(TStreamMsgWithSizeReaderTest, Test5) {
    /* Same as Test3, but 'include_size_field_in_msg' is true. */
    TPipe p(MakePipe());

    /* max message body size is 10 bytes */
    TStreamMsgWithSizeReader<int64_t> r(p.Read, true, true, 10, 32);

    /* write size field with value of 18 */
    int64_t size_field = 0;
    WriteInt64ToHeader(&size_field, 18);
    WritePipe(p.Write, &size_field, 8);

    /* write 10 byte message body */
    WritePipe(p.Write, "1234567890");

    ASSERT_EQ(r.GetState(), TStreamMsgReader::TState::ReadNeeded);

    auto state = r.Read();
    ASSERT_EQ(state, TStreamMsgReader::TState::MsgReady);

    /* verify that ready message has correct contents */
    ASSERT_EQ(MakeReadyMsgStr(r), "1234567890");

    ASSERT_GE(r.GetReadyMsgSize(), 8U);

    for (size_t i = 0; i < 8; ++i) {
      ASSERT_EQ(r.GetReadyMsg()[i], r.GetData()[i]);
    }

    state = r.ConsumeReadyMsg();
    ASSERT_EQ(state, TStreamMsgReader::TState::ReadNeeded);

    /* write size field with value of 19 */
    WriteInt64ToHeader(&size_field, 19);
    WritePipe(p.Write, &size_field, 8);

    /* write 11 byte message body which is > max allowed size */
    WritePipe(p.Write, "1234567890a");

    state = r.Read();
    ASSERT_EQ(state, TStreamMsgReader::TState::DataInvalid);
    ASSERT_TRUE(r.GetDataInvalidReason().IsKnown());
    ASSERT_EQ(*r.GetDataInvalidReason(),
        TStreamMsgWithSizeReaderBase::TDataInvalidReason::MsgBodyTooLarge);
  }

  TEST_F(TStreamMsgWithSizeReaderTest, Test6) {
    /* Same as Test4, but 'include_size_field_in_msg' is true. */
    TPipe p(MakePipe());

    /* max message body size is 10 bytes; this is same as previous test except
       that size_includes_size_field is false */
    TStreamMsgWithSizeReader<int64_t> r(p.Read, false, true, 10, 32);

    /* write size field with value of 10 */
    int64_t size_field = 0;
    WriteInt64ToHeader(&size_field, 10);
    WritePipe(p.Write, &size_field, 8);

    /* write 10 byte message body */
    WritePipe(p.Write, "1234567890");

    ASSERT_EQ(r.GetState(), TStreamMsgReader::TState::ReadNeeded);

    auto state = r.Read();
    ASSERT_EQ(state, TStreamMsgReader::TState::MsgReady);

    /* verify that ready message has correct contents */
    ASSERT_EQ(MakeReadyMsgStr(r), "1234567890");

    ASSERT_GE(r.GetReadyMsgSize(), 8U);

    for (size_t i = 0; i < 8; ++i) {
      ASSERT_EQ(r.GetReadyMsg()[i], r.GetData()[i]);
    }

    state = r.ConsumeReadyMsg();
    ASSERT_EQ(state, TStreamMsgReader::TState::ReadNeeded);

    /* write size field with value of 11 */
    WriteInt64ToHeader(&size_field, 11);
    WritePipe(p.Write, &size_field, 8);

    /* write 11 byte message body which is > max allowed size */
    WritePipe(p.Write, "1234567890a");

    state = r.Read();
    ASSERT_EQ(state, TStreamMsgReader::TState::DataInvalid);
    ASSERT_TRUE(r.GetDataInvalidReason().IsKnown());
    ASSERT_EQ(*r.GetDataInvalidReason(),
        TStreamMsgWithSizeReaderBase::TDataInvalidReason::MsgBodyTooLarge);
  }

  TEST_F(TStreamMsgWithSizeReaderTest, Test7) {
    TPipe p(MakePipe());
    TStreamMsgWithSizeReader<int32_t> r(p.Read, true, false, 10, 32);

    /* write size field with value of -1 */
    int32_t size_field = 0;
    WriteInt32ToHeader(&size_field, -1);
    WritePipe(p.Write, &size_field, 4);

    ASSERT_EQ(r.GetState(), TStreamMsgReader::TState::ReadNeeded);

    auto state = r.Read();

    /* data should be invalid due to negative size field */
    ASSERT_EQ(state, TStreamMsgReader::TState::DataInvalid);
    ASSERT_TRUE(r.GetDataInvalidReason().IsKnown());
    ASSERT_EQ(*r.GetDataInvalidReason(),
        TStreamMsgWithSizeReaderBase::TDataInvalidReason::InvalidSizeField);
  }

  TEST_F(TStreamMsgWithSizeReaderTest, Test8) {
    TPipe p(MakePipe());
    TStreamMsgWithSizeReader<int32_t> r(p.Read, true, false, 10, 32);

    /* write size field with value of 4 */
    int32_t size_field = 0;
    WriteInt32ToHeader(&size_field, 4);
    WritePipe(p.Write, &size_field, 4);

    ASSERT_EQ(r.GetState(), TStreamMsgReader::TState::ReadNeeded);
    auto state = r.Read();
    ASSERT_EQ(state, TStreamMsgReader::TState::MsgReady);

    /* we should get a message with a size of 0 */
    ASSERT_EQ(MakeReadyMsgStr(r), "");

    state = r.ConsumeReadyMsg();
    ASSERT_EQ(state, TStreamMsgReader::TState::ReadNeeded);

    /* write size field with value of 5 */
    WriteInt32ToHeader(&size_field, 5);
    WritePipe(p.Write, &size_field, 4);

    /* write message body */
    WritePipe(p.Write, "x");

    ASSERT_EQ(r.GetState(), TStreamMsgReader::TState::ReadNeeded);
    state = r.Read();
    ASSERT_EQ(state, TStreamMsgReader::TState::MsgReady);

    /* verify that ready message has correct contents */
    ASSERT_EQ(MakeReadyMsgStr(r), "x");

    state = r.ConsumeReadyMsg();
    ASSERT_EQ(state, TStreamMsgReader::TState::ReadNeeded);

    /* write size field with value of 3, which is invalid because the size
       field alone occupies 4 bytes */
    WriteInt32ToHeader(&size_field, 3);
    WritePipe(p.Write, &size_field, 4);

    state = r.Read();
    ASSERT_EQ(state, TStreamMsgReader::TState::DataInvalid);
    ASSERT_TRUE(r.GetDataInvalidReason().IsKnown());
    ASSERT_EQ(*r.GetDataInvalidReason(),
        TStreamMsgWithSizeReaderBase::TDataInvalidReason::InvalidSizeField);
  }

  TEST_F(TStreamMsgWithSizeReaderTest, Test9) {
    TPipe p(MakePipe());
    TStreamMsgWithSizeReader<uint8_t> r(p.Read, false, false, 10, 32);

    /* write size field with value of 5 */
    uint8_t size_field = 5;
    WritePipe(p.Write, &size_field, 1);

    /* write message body */
    WritePipe(p.Write, "abcde");

    ASSERT_EQ(r.GetState(), TStreamMsgReader::TState::ReadNeeded);
    auto state = r.Read();
    ASSERT_EQ(state, TStreamMsgReader::TState::MsgReady);

    /* verify that ready message has correct contents */
    ASSERT_EQ(MakeReadyMsgStr(r), "abcde");

    /* reset reader, so we can make sure it behaves properly after reset */
    TPipe q(MakePipe());
    r.Reset(q.Read);

    /* write size field with value of 3 */
    size_field = 3;
    WritePipe(q.Write, &size_field, 1);

    /* write message body */
    WritePipe(q.Write, "fgh");

    ASSERT_EQ(r.GetState(), TStreamMsgReader::TState::ReadNeeded);
    state = r.Read();
    ASSERT_EQ(state, TStreamMsgReader::TState::MsgReady);

    /* verify that ready message has correct contents */
    ASSERT_EQ(MakeReadyMsgStr(r), "fgh");

    state = r.ConsumeReadyMsg();
    ASSERT_EQ(state, TStreamMsgReader::TState::ReadNeeded);

    /* write size field with value of 2 */
    size_field = 2;
    WritePipe(q.Write, &size_field, 1);

    /* write message body and close write end */
    WritePipe(q.Write, "ij");
    q.CloseWrite();

    state = r.Read();
    ASSERT_EQ(state, TStreamMsgReader::TState::MsgReady);

    /* verify that ready message has correct contents */
    ASSERT_EQ(MakeReadyMsgStr(r), "ij");

    state = r.ConsumeReadyMsg();
    ASSERT_EQ(state, TStreamMsgReader::TState::ReadNeeded);

    state = r.Read();
    ASSERT_EQ(state, TStreamMsgReader::TState::AtEnd);
  }

  TEST_F(TStreamMsgWithSizeReaderTest, Test10) {
    /* test behavior with nonblocking file descriptor */
    TPipe p(MakePipe(true));

    TStreamMsgWithSizeReader<uint8_t> r(p.Read, false, false, 10, 32);

    /* write size field with value of 5 */
    uint8_t size_field = 5;
    WritePipe(p.Write, &size_field, 1);

    /* this will read the size field */
    ASSERT_EQ(r.GetState(), TStreamMsgReader::TState::ReadNeeded);
    auto state = r.Read();
    ASSERT_EQ(state, TStreamMsgReader::TState::ReadNeeded);
    ASSERT_EQ(r.GetDataSize(), 1U);

    /* this will return without reading any more data, since none is available
     */
    state = r.Read();
    ASSERT_EQ(state, TStreamMsgReader::TState::ReadNeeded);
    ASSERT_EQ(r.GetDataSize(), 1U);

    /* write message body and close write end of pipe */
    WritePipe(p.Write, "abcde");
    p.CloseWrite();

    /* this time, reader gets message body */
    ASSERT_EQ(r.GetState(), TStreamMsgReader::TState::ReadNeeded);
    state = r.Read();

    ASSERT_EQ(state, TStreamMsgReader::TState::MsgReady);
    /* verify that ready message has correct contents */
    ASSERT_EQ(MakeReadyMsgStr(r), "abcde");

    state = r.ConsumeReadyMsg();
    ASSERT_EQ(state, TStreamMsgReader::TState::ReadNeeded);

    state = r.Read();
    ASSERT_EQ(state, TStreamMsgReader::TState::AtEnd);
    ASSERT_EQ(r.GetDataSize(), 0U);
  }

  TEST_F(TStreamMsgWithSizeReaderTest, Test11) {
    TPipe p(MakePipe());
    TStreamMsgWithSizeReader<uint8_t> r(p.Read, false, false, 10, 32);

    /* write size field with value of 5 */
    uint8_t size_field = 5;
    WritePipe(p.Write, &size_field, 1);

    /* write message body */
    WritePipe(p.Write, "abcde");

    /* write size field with value of 10, so partial message will remain; then
       close write end of pipe */
    size_field = 10;
    WritePipe(p.Write, &size_field, 1);
    p.CloseWrite();

    ASSERT_EQ(r.GetState(), TStreamMsgReader::TState::ReadNeeded);
    auto state = r.Read();
    ASSERT_EQ(state, TStreamMsgReader::TState::MsgReady);

    /* verify that ready message has correct contents and data size is as
       expected */
    ASSERT_EQ(MakeReadyMsgStr(r), "abcde");
    ASSERT_EQ(r.GetDataSize(), 7U);

    state = r.ConsumeReadyMsg();
    ASSERT_EQ(state, TStreamMsgReader::TState::ReadNeeded);

    state = r.Read();
    ASSERT_EQ(state, TStreamMsgReader::TState::AtEnd);

    /* make sure we have 1 byte of junk left over with the expected value */
    ASSERT_EQ(r.GetDataSize(), 1U);
    ASSERT_EQ(*(r.GetData()), 10);
  }

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  DieOnTerminate();
  return RUN_ALL_TESTS();
}
