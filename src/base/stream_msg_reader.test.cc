/* <base/stream_msg_reader.test.cc>
 
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
 
   Unit test for <base/stream_msg_reader.h>.
 */

#include <base/stream_msg_reader.h>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

#include <fcntl.h>
#include <unistd.h>

#include <base/error_utils.h>
#include <base/opt.h>

#include <gtest/gtest.h>
  
using namespace Base;
 
namespace {

  struct TTestReader : public TStreamMsgReader {
    /* snapshot of reader state on call to one of its subclass-implemented
       methods */
    struct TStateSnapshot {
      size_t CallCount = 0;

      TStreamMsgReader::TState State = TStreamMsgReader::TState::DataInvalid;

      std::string Data;

      bool EndOfInput = false;

      TStateSnapshot() = default;
    };  // TStateSnapshot

    struct TReadyMsgStateSnapshot {
      std::string ReadyMsg;

      size_t ReadyMsgOffset = 0;

      TReadyMsgStateSnapshot() = default;
    };  // TReadyMsgStateSnapshot

    explicit TTestReader(int fd)
        : TStreamMsgReader(fd),
          GetNextMsgReturnValue(
              TOpt<TGetMsgResult>(
                  TStreamMsgReader::TGetMsgResult::Invalid())) {
    }

    TTestReader()
        : TTestReader(-1) {
    }

    ~TTestReader() override = default;

    size_t GetNextReadSize() override;

    TGetMsgResult GetNextMsg() noexcept override;

    void HandleReset() noexcept override;

    void BeforeConsumeReadyMsg() noexcept override;

    void TakeStateSnapshot(TStateSnapshot &snapshot);

    void SetMsgReady(size_t offset, size_t size, size_t trailing_data_size);

    void SetNoMsgReady();

    void SetInvalid();

    TStateSnapshot OnGetNextReadSize;

    TStateSnapshot OnGetNextMsg;

    TStateSnapshot OnHandleReset;

    TStateSnapshot OnBeforeConsumeReadyMsg;

    TReadyMsgStateSnapshot ReadyStateOnBeforeConsumeReadyMsg;

    size_t GetNextReadSizeReturnValue = 0;

    TOpt<TGetMsgResult> GetNextMsgReturnValue;
  };  // TTestReader

  size_t TTestReader::GetNextReadSize() {
    assert(this);
    TakeStateSnapshot(OnGetNextReadSize);
    return GetNextReadSizeReturnValue;
  }

  TStreamMsgReader::TGetMsgResult
  TTestReader::GetNextMsg() noexcept {
    assert(this);
    TakeStateSnapshot(OnGetNextMsg);
    return *GetNextMsgReturnValue;
  }

  void TTestReader::HandleReset() noexcept {
    assert(this);
    TakeStateSnapshot(OnHandleReset);
  }

  void TTestReader::BeforeConsumeReadyMsg() noexcept {
    assert(this);
    TakeStateSnapshot(OnBeforeConsumeReadyMsg);
    size_t size = GetReadyMsgSize();

    if (size == 0) {
      ReadyStateOnBeforeConsumeReadyMsg.ReadyMsg.clear();
    } else {
      ReadyStateOnBeforeConsumeReadyMsg.ReadyMsg.assign(
          reinterpret_cast<const char *>(GetReadyMsg()), size);
    }

    ReadyStateOnBeforeConsumeReadyMsg.ReadyMsgOffset = GetReadyMsgOffset();
  }

  void TTestReader::TakeStateSnapshot(TStateSnapshot &snapshot) {
    assert(this);
    snapshot.State = GetState();
    size_t size = GetDataSize();

    if (size == 0) {
      snapshot.Data.clear();
    } else {
      snapshot.Data.assign(reinterpret_cast<const char *>(GetData()),
          GetDataSize());
    }

    snapshot.EndOfInput = AtEndOfInput();
    ++snapshot.CallCount;
  }

  void TTestReader::SetMsgReady(size_t offset, size_t size,
      size_t trailing_data_size) {
    assert(this);
    GetNextMsgReturnValue.Reset();
    GetNextMsgReturnValue.MakeKnown(TGetMsgResult::MsgReady(offset, size,
        trailing_data_size));
  }

  void TTestReader::SetNoMsgReady() {
    assert(this);
    GetNextMsgReturnValue.Reset();
    GetNextMsgReturnValue.MakeKnown(TGetMsgResult::NoMsgReady());
  }

  void TTestReader::SetInvalid() {
    assert(this);
    GetNextMsgReturnValue.Reset();
    GetNextMsgReturnValue.MakeKnown(TGetMsgResult::Invalid());
  }

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
        close(Read);
        Read = -1;
      }
    }

    void CloseWrite() {
      assert(this);

      if (Write >= 0) {
        close(Write);
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
    IfLt0(pipe2(pipefd, nonblocking ? O_NONBLOCK : 0));
    return TPipe(pipefd[0], pipefd[1]);
  }

  void WritePipe(int pipefd, const std::string &s) {
    ssize_t bytes = IfLt0(write(pipefd, s.data(), s.size()));

    if (static_cast<size_t>(bytes) != s.size()) {
      /* The amounts of data written in these tests will be small enough that
         the entire write should always complete.  Therefore we should never
         get here.  Just in case we do, throw an exception because if the
         assumption that all bytes are written is ever violated, tests will
         fail. */
      throw std::logic_error("Short pipe write in test for TStreamMsgReader");
    }
  }

  std::string MakeReadyMsgStr(const TStreamMsgReader &r) {
    if (r.GetReadyMsgSize() == 0) {
      return "";
    }

    return std::string(reinterpret_cast<const char *>(r.GetReadyMsg()),
        r.GetReadyMsgSize());
  }

  std::string MakeDataStr(const TStreamMsgReader &r) {
    if (r.GetDataSize() == 0) {
      return "";
    }

    return std::string(reinterpret_cast<const char *>(r.GetData()),
        r.GetDataSize());
  }

  /* The fixture for testing class TStreamMsgReader. */
  class TStreamMsgReaderTest : public ::testing::Test {
    protected:
    TStreamMsgReaderTest() = default;

    ~TStreamMsgReaderTest() override = default;

    void SetUp() override {
    }

    void TearDown() override {
    }
  };  // TStreamMsgReaderTest


  TEST_F(TStreamMsgReaderTest, Test1) {
    TTestReader r;

    /* check initial state of reader */
    ASSERT_EQ(r.GetState(), TStreamMsgReader::TState::AtEnd);
    ASSERT_EQ(r.GetFd(), -1);
    ASSERT_EQ(r.GetDataSize(), 0U);
    TPipe p(MakePipe());

    /* check reset behavior with no FD passed in */
    ASSERT_EQ(r.OnHandleReset.CallCount, 0U);
    r.Reset();
    ASSERT_EQ(r.OnHandleReset.CallCount, 1U);
    ASSERT_EQ(r.GetFd(), -1);
    ASSERT_EQ(r.GetState(), TStreamMsgReader::TState::AtEnd);

    /* check reset behavior with fd passed in */
    r.Reset(p.Read);
    ASSERT_EQ(r.OnHandleReset.CallCount, 2U);
    ASSERT_EQ(r.GetFd(), p.Read);
    ASSERT_EQ(r.GetState(), TStreamMsgReader::TState::ReadNeeded);

    /* create some message data for the reader to read */
    WritePipe(p.Write, "xxx");

    /* reader should try to read 10 bytes, get 3 bytes, and not find a ready
       message */
    r.GetNextReadSizeReturnValue = 10;
    r.SetNoMsgReady();

    /* verify expected Read() behavior */
    ASSERT_EQ(r.OnGetNextMsg.CallCount, 0U);
    auto state = r.Read();
    ASSERT_EQ(state, TStreamMsgReader::TState::ReadNeeded);
    ASSERT_EQ(r.OnGetNextMsg.CallCount, 1U);
    ASSERT_EQ(r.OnGetNextMsg.State, TStreamMsgReader::TState::ReadNeeded);
    ASSERT_EQ(r.OnGetNextMsg.Data, "xxx");
    ASSERT_FALSE(r.OnGetNextMsg.EndOfInput);

    /* write remaining message data and close write end of pipe */
    WritePipe(p.Write, "yyyyyzzzzaaaaabbbbbcccddeeefffffgg");
    p.CloseWrite();

    /* reader should read next 9 bytes, and find a ready message 5 bytes long,
       with a header of 3 bytes and trailer of 4 bytes */
    r.GetNextReadSizeReturnValue = 9;
    r.SetMsgReady(3, 5, 4);

    /* verify expected Read() behavior */
    ASSERT_EQ(r.OnGetNextMsg.CallCount, 1U);
    state = r.Read();
    ASSERT_EQ(state, TStreamMsgReader::TState::MsgReady);
    ASSERT_EQ(r.OnGetNextMsg.CallCount, 2U);
    ASSERT_EQ(r.OnGetNextMsg.State, TStreamMsgReader::TState::ReadNeeded);
    ASSERT_EQ(r.OnGetNextMsg.Data, "xxxyyyyyzzzz");
    ASSERT_FALSE(r.OnGetNextMsg.EndOfInput);
    ASSERT_EQ(MakeReadyMsgStr(r), "yyyyy");

    /* mark message as consumed and verify expected state in
       BeforeConsumeReadyMsg() */
    r.SetNoMsgReady();
    ASSERT_EQ(r.OnBeforeConsumeReadyMsg.CallCount, 0U);
    ASSERT_EQ(r.OnGetNextMsg.CallCount, 2U);
    state = r.ConsumeReadyMsg();
    ASSERT_EQ(r.OnBeforeConsumeReadyMsg.CallCount, 1U);
    ASSERT_EQ(r.OnGetNextMsg.CallCount, 3U);
    ASSERT_EQ(state, TStreamMsgReader::TState::ReadNeeded);
    ASSERT_EQ(r.OnBeforeConsumeReadyMsg.State,
        TStreamMsgReader::TState::MsgReady);
    ASSERT_EQ(r.OnBeforeConsumeReadyMsg.Data, "xxxyyyyyzzzz");
    ASSERT_FALSE(r.OnBeforeConsumeReadyMsg.EndOfInput);
    ASSERT_EQ(r.ReadyStateOnBeforeConsumeReadyMsg.ReadyMsg, "yyyyy");
    ASSERT_EQ(r.ReadyStateOnBeforeConsumeReadyMsg.ReadyMsgOffset, 3U);
    ASSERT_EQ(r.OnGetNextMsg.State, TStreamMsgReader::TState::MsgReady);
    ASSERT_EQ(r.OnGetNextMsg.Data, "");
    ASSERT_FALSE(r.OnGetNextMsg.EndOfInput);

    /* reader should read the next 13 bytes, and find a ready message 5 bytes
       long, with a header of 5 bytes and no trailer */
    r.GetNextReadSizeReturnValue = 13;
    r.SetMsgReady(5, 5, 0);

    /* verify expected Read() behavior */
    ASSERT_EQ(r.OnGetNextMsg.CallCount, 3U);
    state = r.Read();
    ASSERT_EQ(state, TStreamMsgReader::TState::MsgReady);
    ASSERT_EQ(r.OnGetNextMsg.CallCount, 4U);
    ASSERT_EQ(r.OnGetNextMsg.State, TStreamMsgReader::TState::ReadNeeded);
    ASSERT_EQ(r.OnGetNextMsg.Data, "aaaaabbbbbccc");
    ASSERT_FALSE(r.OnGetNextMsg.EndOfInput);
    ASSERT_EQ(MakeReadyMsgStr(r), "bbbbb");

    /* mark message as consumed and verify expected state in
       BeforeConsumeReadyMsg() */
    r.SetNoMsgReady();
    ASSERT_EQ(r.OnBeforeConsumeReadyMsg.CallCount, 1U);
    ASSERT_EQ(r.OnGetNextMsg.CallCount, 4U);
    state = r.ConsumeReadyMsg();
    ASSERT_EQ(r.OnBeforeConsumeReadyMsg.CallCount, 2U);
    ASSERT_EQ(r.OnGetNextMsg.CallCount, 5U);
    ASSERT_EQ(state, TStreamMsgReader::TState::ReadNeeded);
    ASSERT_EQ(r.OnBeforeConsumeReadyMsg.State,
        TStreamMsgReader::TState::MsgReady);
    ASSERT_EQ(r.OnBeforeConsumeReadyMsg.Data, "aaaaabbbbbccc");
    ASSERT_FALSE(r.OnBeforeConsumeReadyMsg.EndOfInput);
    ASSERT_EQ(r.ReadyStateOnBeforeConsumeReadyMsg.ReadyMsg, "bbbbb");
    ASSERT_EQ(r.ReadyStateOnBeforeConsumeReadyMsg.ReadyMsgOffset, 5U);
    ASSERT_EQ(r.OnGetNextMsg.State, TStreamMsgReader::TState::MsgReady);
    ASSERT_EQ(r.OnGetNextMsg.Data, "ccc");
    ASSERT_FALSE(r.OnGetNextMsg.EndOfInput);

    /* reader should read the next 5 bytes, and find a ready message 3 bytes
       long, with no header and a trailer of 2 bytes */
    r.GetNextReadSizeReturnValue = 5;
    r.SetMsgReady(0, 3, 2);

    /* verify expected Read() behavior */
    ASSERT_EQ(r.OnGetNextMsg.CallCount, 5U);
    state = r.Read();
    ASSERT_EQ(state, TStreamMsgReader::TState::MsgReady);
    ASSERT_EQ(r.OnGetNextMsg.CallCount, 6U);
    ASSERT_EQ(r.OnGetNextMsg.State, TStreamMsgReader::TState::ReadNeeded);
    ASSERT_EQ(r.OnGetNextMsg.Data, "cccddeee");
    ASSERT_FALSE(r.OnGetNextMsg.EndOfInput);
    ASSERT_EQ(MakeReadyMsgStr(r), "ccc");

    /* mark message as consumed and verify expected state in
       BeforeConsumeReadyMsg() */
    r.SetNoMsgReady();
    ASSERT_EQ(r.OnBeforeConsumeReadyMsg.CallCount, 2U);
    ASSERT_EQ(r.OnGetNextMsg.CallCount, 6U);
    state = r.ConsumeReadyMsg();
    ASSERT_EQ(r.OnBeforeConsumeReadyMsg.CallCount, 3U);
    ASSERT_EQ(r.OnGetNextMsg.CallCount, 7U);
    ASSERT_EQ(state, TStreamMsgReader::TState::ReadNeeded);
    ASSERT_EQ(r.OnBeforeConsumeReadyMsg.State,
        TStreamMsgReader::TState::MsgReady);
    ASSERT_EQ(r.OnBeforeConsumeReadyMsg.Data, "cccddeee");
    ASSERT_FALSE(r.OnBeforeConsumeReadyMsg.EndOfInput);
    ASSERT_EQ(r.ReadyStateOnBeforeConsumeReadyMsg.ReadyMsg, "ccc");
    ASSERT_EQ(r.ReadyStateOnBeforeConsumeReadyMsg.ReadyMsgOffset, 0U);
    ASSERT_EQ(r.OnGetNextMsg.State, TStreamMsgReader::TState::MsgReady);
    ASSERT_EQ(r.OnGetNextMsg.Data, "eee");
    ASSERT_FALSE(r.OnGetNextMsg.EndOfInput);

    /* reader should read the next 5 bytes, and find a ready message 3 bytes
       long, with no header and no trailer */
    r.GetNextReadSizeReturnValue = 5;
    r.SetMsgReady(0, 3, 0);

    /* verify expected Read() behavior */
    ASSERT_EQ(r.OnGetNextMsg.CallCount, 7U);
    state = r.Read();
    ASSERT_EQ(state, TStreamMsgReader::TState::MsgReady);
    ASSERT_EQ(r.OnGetNextMsg.CallCount, 8U);
    ASSERT_EQ(r.OnGetNextMsg.State, TStreamMsgReader::TState::ReadNeeded);
    ASSERT_EQ(r.OnGetNextMsg.Data, "eeefffff");
    ASSERT_FALSE(r.OnGetNextMsg.EndOfInput);
    ASSERT_EQ(MakeReadyMsgStr(r), "eee");

    /* mark message as consumed and verify expected state in
       BeforeConsumeReadyMsg() */
    r.SetNoMsgReady();
    ASSERT_EQ(r.OnBeforeConsumeReadyMsg.CallCount, 3U);
    ASSERT_EQ(r.OnGetNextMsg.CallCount, 8U);
    state = r.ConsumeReadyMsg();
    ASSERT_EQ(r.OnBeforeConsumeReadyMsg.CallCount, 4U);
    ASSERT_EQ(r.OnGetNextMsg.CallCount, 9U);
    ASSERT_EQ(state, TStreamMsgReader::TState::ReadNeeded);
    ASSERT_EQ(r.OnBeforeConsumeReadyMsg.State,
        TStreamMsgReader::TState::MsgReady);
    ASSERT_EQ(r.OnBeforeConsumeReadyMsg.Data, "eeefffff");
    ASSERT_FALSE(r.OnBeforeConsumeReadyMsg.EndOfInput);
    ASSERT_EQ(r.ReadyStateOnBeforeConsumeReadyMsg.ReadyMsg, "eee");
    ASSERT_EQ(r.ReadyStateOnBeforeConsumeReadyMsg.ReadyMsgOffset, 0U);
    ASSERT_EQ(r.OnGetNextMsg.State, TStreamMsgReader::TState::MsgReady);
    ASSERT_EQ(r.OnGetNextMsg.Data, "fffff");
    ASSERT_FALSE(r.OnGetNextMsg.EndOfInput);

    /* reader should read the next 2 bytes, and find a ready message 2 bytes
       long, with a header of 5 bytes and no trailer */
    r.GetNextReadSizeReturnValue = 2;
    r.SetMsgReady(5, 2, 0);

    /* verify expected Read() behavior */
    ASSERT_EQ(r.OnGetNextMsg.CallCount, 9U);
    state = r.Read();
    ASSERT_EQ(state, TStreamMsgReader::TState::MsgReady);
    ASSERT_EQ(r.OnGetNextMsg.CallCount, 10U);
    ASSERT_EQ(r.OnGetNextMsg.State, TStreamMsgReader::TState::ReadNeeded);
    ASSERT_EQ(r.OnGetNextMsg.Data, "fffffgg");
    ASSERT_FALSE(r.OnGetNextMsg.EndOfInput);
    ASSERT_EQ(MakeReadyMsgStr(r), "gg");

    /* mark message as consumed and verify expected state in
       BeforeConsumeReadyMsg() */
    r.SetNoMsgReady();
    ASSERT_EQ(r.OnBeforeConsumeReadyMsg.CallCount, 4U);
    ASSERT_EQ(r.OnGetNextMsg.CallCount, 10U);
    state = r.ConsumeReadyMsg();
    ASSERT_EQ(r.OnBeforeConsumeReadyMsg.CallCount, 5U);
    ASSERT_EQ(r.OnGetNextMsg.CallCount, 11U);
    ASSERT_EQ(state, TStreamMsgReader::TState::ReadNeeded);
    ASSERT_EQ(r.OnBeforeConsumeReadyMsg.State,
        TStreamMsgReader::TState::MsgReady);
    ASSERT_EQ(r.OnBeforeConsumeReadyMsg.Data, "fffffgg");
    ASSERT_FALSE(r.OnBeforeConsumeReadyMsg.EndOfInput);
    ASSERT_EQ(r.ReadyStateOnBeforeConsumeReadyMsg.ReadyMsg, "gg");
    ASSERT_EQ(r.ReadyStateOnBeforeConsumeReadyMsg.ReadyMsgOffset, 5U);
    ASSERT_EQ(r.OnGetNextMsg.State, TStreamMsgReader::TState::MsgReady);
    ASSERT_EQ(r.OnGetNextMsg.Data, "");
    ASSERT_FALSE(r.OnGetNextMsg.EndOfInput);

    /* reader should try to read another byte and find that there is no more
       data */
    r.GetNextReadSizeReturnValue = 1;
    r.SetNoMsgReady();

    /* verify expected Read() behavior */
    ASSERT_EQ(r.OnGetNextMsg.CallCount, 11U);
    state = r.Read();
    ASSERT_EQ(state, TStreamMsgReader::TState::AtEnd);
    ASSERT_EQ(r.OnGetNextMsg.CallCount, 12U);
    ASSERT_EQ(r.OnGetNextMsg.State, TStreamMsgReader::TState::ReadNeeded);
    ASSERT_EQ(r.OnGetNextMsg.Data, "");
    ASSERT_TRUE(r.OnGetNextMsg.EndOfInput);

    /* verify that there is no left over data */
    ASSERT_EQ(r.GetDataSize(), 0U);

    TPipe q(MakePipe());

    /* reset reader with new FD and make sure it still behaves properly */
    r.Reset(q.Read);
    ASSERT_EQ(r.OnHandleReset.CallCount, 3U);
    ASSERT_EQ(r.GetFd(), q.Read);
    ASSERT_EQ(r.GetState(), TStreamMsgReader::TState::ReadNeeded);

    /* create some message data for the reader to read and then close write end
       of pipe */
    WritePipe(q.Write, "hhhiijkkmmm");
    q.CloseWrite();

    /* reader should read next 5 bytes, and find a ready message 2 bytes long,
       with a header of 3 bytes and trailer of 0 bytes */
    r.GetNextReadSizeReturnValue = 5;
    r.SetMsgReady(3, 2, 0);

    /* verify expected Read() behavior */
    ASSERT_EQ(r.OnGetNextMsg.CallCount, 12U);
    state = r.Read();
    ASSERT_EQ(state, TStreamMsgReader::TState::MsgReady);
    ASSERT_EQ(r.OnGetNextMsg.CallCount, 13U);
    ASSERT_EQ(r.OnGetNextMsg.State, TStreamMsgReader::TState::ReadNeeded);
    ASSERT_EQ(r.OnGetNextMsg.Data, "hhhii");
    ASSERT_FALSE(r.OnGetNextMsg.EndOfInput);
    ASSERT_EQ(MakeReadyMsgStr(r), "ii");

    /* mark message as consumed and verify expected state in
       BeforeConsumeReadyMsg() */
    r.SetNoMsgReady();
    ASSERT_EQ(r.OnBeforeConsumeReadyMsg.CallCount, 5U);
    ASSERT_EQ(r.OnGetNextMsg.CallCount, 13U);
    state = r.ConsumeReadyMsg();
    ASSERT_EQ(r.OnBeforeConsumeReadyMsg.CallCount, 6U);
    ASSERT_EQ(r.OnGetNextMsg.CallCount, 14U);
    ASSERT_EQ(state, TStreamMsgReader::TState::ReadNeeded);
    ASSERT_EQ(r.OnBeforeConsumeReadyMsg.State,
        TStreamMsgReader::TState::MsgReady);
    ASSERT_EQ(r.OnBeforeConsumeReadyMsg.Data, "hhhii");
    ASSERT_FALSE(r.OnBeforeConsumeReadyMsg.EndOfInput);
    ASSERT_EQ(r.ReadyStateOnBeforeConsumeReadyMsg.ReadyMsg, "ii");
    ASSERT_EQ(r.ReadyStateOnBeforeConsumeReadyMsg.ReadyMsgOffset, 3U);
    ASSERT_EQ(r.OnGetNextMsg.State, TStreamMsgReader::TState::MsgReady);
    ASSERT_EQ(r.OnGetNextMsg.Data, "");
    ASSERT_FALSE(r.OnGetNextMsg.EndOfInput);

    /* reader should try to read next 10 bytes, get only 6 bytes, and find a
       ready message 2 bytes long with a header of 1 byte and trailer of 0
       bytes */
    r.GetNextReadSizeReturnValue = 10;
    r.SetMsgReady(1, 2, 0);

    /* verify expected Read() behavior */
    ASSERT_EQ(r.OnGetNextMsg.CallCount, 14U);
    state = r.Read();
    ASSERT_EQ(state, TStreamMsgReader::TState::MsgReady);
    ASSERT_EQ(r.OnGetNextMsg.CallCount, 15U);
    ASSERT_EQ(r.OnGetNextMsg.State, TStreamMsgReader::TState::ReadNeeded);
    ASSERT_EQ(r.OnGetNextMsg.Data, "jkkmmm");
    ASSERT_FALSE(r.OnGetNextMsg.EndOfInput);
    ASSERT_EQ(MakeReadyMsgStr(r), "kk");

    /* mark message as consumed and verify expected state in
       BeforeConsumeReadyMsg() */
    r.SetNoMsgReady();
    ASSERT_EQ(r.OnBeforeConsumeReadyMsg.CallCount, 6U);
    ASSERT_EQ(r.OnGetNextMsg.CallCount, 15U);
    state = r.ConsumeReadyMsg();
    ASSERT_EQ(r.OnBeforeConsumeReadyMsg.CallCount, 7U);
    ASSERT_EQ(r.OnGetNextMsg.CallCount, 16U);
    ASSERT_EQ(state, TStreamMsgReader::TState::ReadNeeded);
    ASSERT_EQ(r.OnBeforeConsumeReadyMsg.State,
        TStreamMsgReader::TState::MsgReady);
    ASSERT_EQ(r.OnBeforeConsumeReadyMsg.Data, "jkkmmm");
    ASSERT_FALSE(r.OnBeforeConsumeReadyMsg.EndOfInput);
    ASSERT_EQ(r.ReadyStateOnBeforeConsumeReadyMsg.ReadyMsg, "kk");
    ASSERT_EQ(r.ReadyStateOnBeforeConsumeReadyMsg.ReadyMsgOffset, 1U);
    ASSERT_EQ(r.OnGetNextMsg.State, TStreamMsgReader::TState::MsgReady);
    ASSERT_EQ(r.OnGetNextMsg.Data, "mmm");
    ASSERT_FALSE(r.OnGetNextMsg.EndOfInput);

    /* reader should try to read another 10 bytes and find that there is no
       more data */
    r.GetNextReadSizeReturnValue = 10;
    r.SetNoMsgReady();

    /* verify expected Read() behavior */
    ASSERT_EQ(r.OnGetNextMsg.CallCount, 16U);
    state = r.Read();
    ASSERT_EQ(state, TStreamMsgReader::TState::AtEnd);
    ASSERT_EQ(r.OnGetNextMsg.CallCount, 17U);
    ASSERT_EQ(r.OnGetNextMsg.State, TStreamMsgReader::TState::ReadNeeded);
    ASSERT_EQ(r.OnGetNextMsg.Data, "mmm");
    ASSERT_TRUE(r.OnGetNextMsg.EndOfInput);

    /* verify that there are 3 bytes of left over data */
    ASSERT_EQ(MakeDataStr(r), "mmm");
    ASSERT_EQ(r.GetDataSize(), 3U);

    TPipe qq(MakePipe());

    /* reset reader with new FD and make sure it still behaves properly even
       when there was extra data left over */
    r.Reset(qq.Read);
    ASSERT_EQ(r.OnHandleReset.CallCount, 4U);
    ASSERT_EQ(r.GetFd(), qq.Read);
    ASSERT_EQ(r.GetState(), TStreamMsgReader::TState::ReadNeeded);

    /* create some message data for the reader to read and then close write end
       of pipe */
    WritePipe(qq.Write, "nnooooop");
    qq.CloseWrite();

    /* reader should try to read next 20 bytes, get the entire data of 8 bytes,
       and find a ready message 5 bytes long,
       with a header of 2 bytes and trailer of 1 byte */
    r.GetNextReadSizeReturnValue = 20;
    r.SetMsgReady(2, 5, 1);

    /* verify expected Read() behavior */
    ASSERT_EQ(r.OnGetNextMsg.CallCount, 17U);
    state = r.Read();
    ASSERT_EQ(state, TStreamMsgReader::TState::MsgReady);
    ASSERT_EQ(r.OnGetNextMsg.CallCount, 18U);
    ASSERT_EQ(r.OnGetNextMsg.State, TStreamMsgReader::TState::ReadNeeded);
    ASSERT_EQ(r.OnGetNextMsg.Data, "nnooooop");
    ASSERT_FALSE(r.OnGetNextMsg.EndOfInput);
    ASSERT_EQ(MakeReadyMsgStr(r), "ooooo");

    /* mark message as consumed and verify expected state in
       BeforeConsumeReadyMsg() */
    r.SetNoMsgReady();
    ASSERT_EQ(r.OnBeforeConsumeReadyMsg.CallCount, 7U);
    ASSERT_EQ(r.OnGetNextMsg.CallCount, 18U);
    state = r.ConsumeReadyMsg();
    ASSERT_EQ(r.OnBeforeConsumeReadyMsg.CallCount, 8U);
    ASSERT_EQ(r.OnGetNextMsg.CallCount, 19U);
    ASSERT_EQ(state, TStreamMsgReader::TState::ReadNeeded);
    ASSERT_EQ(r.OnBeforeConsumeReadyMsg.State,
        TStreamMsgReader::TState::MsgReady);
    ASSERT_EQ(r.OnBeforeConsumeReadyMsg.Data, "nnooooop");
    ASSERT_FALSE(r.OnBeforeConsumeReadyMsg.EndOfInput);
    ASSERT_EQ(r.ReadyStateOnBeforeConsumeReadyMsg.ReadyMsg, "ooooo");
    ASSERT_EQ(r.ReadyStateOnBeforeConsumeReadyMsg.ReadyMsgOffset, 2U);
    ASSERT_EQ(r.OnGetNextMsg.State, TStreamMsgReader::TState::MsgReady);
    ASSERT_EQ(r.OnGetNextMsg.Data, "");
    ASSERT_FALSE(r.OnGetNextMsg.EndOfInput);

    /* reader should try to read another 20 bytes and find that there is no
       more data */
    r.GetNextReadSizeReturnValue = 20;
    r.SetNoMsgReady();

    /* verify expected Read() behavior */
    ASSERT_EQ(r.OnGetNextMsg.CallCount, 19U);
    state = r.Read();
    ASSERT_EQ(state, TStreamMsgReader::TState::AtEnd);
    ASSERT_EQ(r.OnGetNextMsg.CallCount, 20U);
    ASSERT_EQ(r.OnGetNextMsg.State, TStreamMsgReader::TState::ReadNeeded);
    ASSERT_EQ(r.OnGetNextMsg.Data, "");
    ASSERT_TRUE(r.OnGetNextMsg.EndOfInput);

    /* verify that there are 0 bytes of left over data */
    ASSERT_EQ(MakeDataStr(r), "");
    ASSERT_EQ(r.GetDataSize(), 0U);
  }

  TEST_F(TStreamMsgReaderTest, Test2) {
    TPipe p(MakePipe());
    TTestReader r(p.Read);

    /* check initial state of reader */
    ASSERT_EQ(r.GetState(), TStreamMsgReader::TState::ReadNeeded);
    ASSERT_EQ(r.GetFd(), p.Read);
    ASSERT_EQ(r.GetDataSize(), 0U);

    /* create some message data for the reader to read and then close write end
       of pipe */
    WritePipe(p.Write, "xxxyyyyyzzzzaaaaabbbbbcccddeeefffffgg");
    p.CloseWrite();

    /* reader should attempt to read 100 bytes, get all of the data, and find a
       ready message 5 bytes long, with a header of 3 bytes and trailer of 4
       bytes */
    r.GetNextReadSizeReturnValue = 100;
    r.SetMsgReady(3, 5, 4);

    /* verify expected Read() behavior */
    ASSERT_EQ(r.OnGetNextMsg.CallCount, 0U);
    auto state = r.Read();
    ASSERT_EQ(state, TStreamMsgReader::TState::MsgReady);
    ASSERT_EQ(r.OnGetNextMsg.CallCount, 1U);
    ASSERT_EQ(r.OnGetNextMsg.State, TStreamMsgReader::TState::ReadNeeded);
    ASSERT_EQ(r.OnGetNextMsg.Data, "xxxyyyyyzzzzaaaaabbbbbcccddeeefffffgg");
    ASSERT_FALSE(r.OnGetNextMsg.EndOfInput);
    ASSERT_EQ(MakeReadyMsgStr(r), "yyyyy");

    /* mark message as consumed and verify expected state in
       BeforeConsumeReadyMsg(); next message is 5 bytes long with a header of
       5 bytes and a trailer of 0 bytes */
    r.SetMsgReady(5, 5, 0);
    ASSERT_EQ(r.OnBeforeConsumeReadyMsg.CallCount, 0U);
    ASSERT_EQ(r.OnGetNextMsg.CallCount, 1U);
    state = r.ConsumeReadyMsg();
    ASSERT_EQ(r.OnBeforeConsumeReadyMsg.CallCount, 1U);
    ASSERT_EQ(r.OnGetNextMsg.CallCount, 2U);
    ASSERT_EQ(state, TStreamMsgReader::TState::MsgReady);
    ASSERT_EQ(r.OnBeforeConsumeReadyMsg.State,
        TStreamMsgReader::TState::MsgReady);
    ASSERT_EQ(r.OnBeforeConsumeReadyMsg.Data,
        "xxxyyyyyzzzzaaaaabbbbbcccddeeefffffgg");
    ASSERT_FALSE(r.OnBeforeConsumeReadyMsg.EndOfInput);
    ASSERT_EQ(r.ReadyStateOnBeforeConsumeReadyMsg.ReadyMsg, "yyyyy");
    ASSERT_EQ(r.ReadyStateOnBeforeConsumeReadyMsg.ReadyMsgOffset, 3U);
    ASSERT_EQ(r.OnGetNextMsg.State, TStreamMsgReader::TState::MsgReady);
    ASSERT_EQ(r.OnGetNextMsg.Data, "aaaaabbbbbcccddeeefffffgg");
    ASSERT_FALSE(r.OnGetNextMsg.EndOfInput);
    ASSERT_EQ(MakeReadyMsgStr(r), "bbbbb");

    /* mark message as consumed and verify expected state in
       BeforeConsumeReadyMsg(); next message is 3 bytes long with no header and
       a trailer of 2 bytes */
    r.SetMsgReady(0, 3, 2);
    ASSERT_EQ(r.OnBeforeConsumeReadyMsg.CallCount, 1U);
    ASSERT_EQ(r.OnGetNextMsg.CallCount, 2U);
    state = r.ConsumeReadyMsg();
    ASSERT_EQ(r.OnBeforeConsumeReadyMsg.CallCount, 2U);
    ASSERT_EQ(r.OnGetNextMsg.CallCount, 3U);
    ASSERT_EQ(state, TStreamMsgReader::TState::MsgReady);
    ASSERT_EQ(r.OnBeforeConsumeReadyMsg.State,
        TStreamMsgReader::TState::MsgReady);
    ASSERT_EQ(r.OnBeforeConsumeReadyMsg.Data, "aaaaabbbbbcccddeeefffffgg");
    ASSERT_FALSE(r.OnBeforeConsumeReadyMsg.EndOfInput);
    ASSERT_EQ(r.ReadyStateOnBeforeConsumeReadyMsg.ReadyMsg, "bbbbb");
    ASSERT_EQ(r.ReadyStateOnBeforeConsumeReadyMsg.ReadyMsgOffset, 5U);
    ASSERT_EQ(r.OnGetNextMsg.State, TStreamMsgReader::TState::MsgReady);
    ASSERT_EQ(r.OnGetNextMsg.Data, "cccddeeefffffgg");
    ASSERT_FALSE(r.OnGetNextMsg.EndOfInput);
    ASSERT_EQ(MakeReadyMsgStr(r), "ccc");

    /* mark message as consumed and verify expected state in
       BeforeConsumeReadyMsg(); next message is 3 bytes long with no header and
       no trailer */
    r.SetMsgReady(0, 3, 0);
    ASSERT_EQ(r.OnBeforeConsumeReadyMsg.CallCount, 2U);
    ASSERT_EQ(r.OnGetNextMsg.CallCount, 3U);
    state = r.ConsumeReadyMsg();
    ASSERT_EQ(r.OnBeforeConsumeReadyMsg.CallCount, 3U);
    ASSERT_EQ(r.OnGetNextMsg.CallCount, 4U);
    ASSERT_EQ(state, TStreamMsgReader::TState::MsgReady);
    ASSERT_EQ(r.OnBeforeConsumeReadyMsg.State,
        TStreamMsgReader::TState::MsgReady);
    ASSERT_EQ(r.OnBeforeConsumeReadyMsg.Data, "cccddeeefffffgg");
    ASSERT_FALSE(r.OnBeforeConsumeReadyMsg.EndOfInput);
    ASSERT_EQ(r.ReadyStateOnBeforeConsumeReadyMsg.ReadyMsg, "ccc");
    ASSERT_EQ(r.ReadyStateOnBeforeConsumeReadyMsg.ReadyMsgOffset, 0U);
    ASSERT_EQ(r.OnGetNextMsg.State, TStreamMsgReader::TState::MsgReady);
    ASSERT_EQ(r.OnGetNextMsg.Data, "eeefffffgg");
    ASSERT_FALSE(r.OnGetNextMsg.EndOfInput);
    ASSERT_EQ(MakeReadyMsgStr(r), "eee");

    /* mark message as consumed and verify expected state in
       BeforeConsumeReadyMsg(); next message is 2 bytes long with a header of
       5 bytes and no trailer */
    r.SetMsgReady(5, 2, 0);
    ASSERT_EQ(r.OnBeforeConsumeReadyMsg.CallCount, 3U);
    ASSERT_EQ(r.OnGetNextMsg.CallCount, 4U);
    state = r.ConsumeReadyMsg();
    ASSERT_EQ(r.OnBeforeConsumeReadyMsg.CallCount, 4U);
    ASSERT_EQ(r.OnGetNextMsg.CallCount, 5U);
    ASSERT_EQ(state, TStreamMsgReader::TState::MsgReady);
    ASSERT_EQ(r.OnBeforeConsumeReadyMsg.State,
        TStreamMsgReader::TState::MsgReady);
    ASSERT_EQ(r.OnBeforeConsumeReadyMsg.Data, "eeefffffgg");
    ASSERT_FALSE(r.OnBeforeConsumeReadyMsg.EndOfInput);
    ASSERT_EQ(r.ReadyStateOnBeforeConsumeReadyMsg.ReadyMsg, "eee");
    ASSERT_EQ(r.ReadyStateOnBeforeConsumeReadyMsg.ReadyMsgOffset, 0U);
    ASSERT_EQ(r.OnGetNextMsg.State, TStreamMsgReader::TState::MsgReady);
    ASSERT_EQ(r.OnGetNextMsg.Data, "fffffgg");
    ASSERT_FALSE(r.OnGetNextMsg.EndOfInput);
    ASSERT_EQ(MakeReadyMsgStr(r), "gg");

    /* mark message as consumed and verify expected state in
       BeforeConsumeReadyMsg() */
    r.SetNoMsgReady();
    ASSERT_EQ(r.OnBeforeConsumeReadyMsg.CallCount, 4U);
    ASSERT_EQ(r.OnGetNextMsg.CallCount, 5U);
    state = r.ConsumeReadyMsg();
    ASSERT_EQ(r.OnBeforeConsumeReadyMsg.CallCount, 5U);
    ASSERT_EQ(r.OnGetNextMsg.CallCount, 6U);
    ASSERT_EQ(state, TStreamMsgReader::TState::ReadNeeded);
    ASSERT_EQ(r.OnBeforeConsumeReadyMsg.State,
        TStreamMsgReader::TState::MsgReady);
    ASSERT_EQ(r.OnBeforeConsumeReadyMsg.Data, "fffffgg");
    ASSERT_FALSE(r.OnBeforeConsumeReadyMsg.EndOfInput);
    ASSERT_EQ(r.ReadyStateOnBeforeConsumeReadyMsg.ReadyMsg, "gg");
    ASSERT_EQ(r.ReadyStateOnBeforeConsumeReadyMsg.ReadyMsgOffset, 5U);
    ASSERT_EQ(r.OnGetNextMsg.State, TStreamMsgReader::TState::MsgReady);
    ASSERT_EQ(r.OnGetNextMsg.Data, "");
    ASSERT_FALSE(r.OnGetNextMsg.EndOfInput);

    /* reader should try to read another 100 bytes and find that there is no
       more data */
    r.GetNextReadSizeReturnValue = 100;
    r.SetNoMsgReady();

    /* verify expected Read() behavior */
    ASSERT_EQ(r.OnGetNextMsg.CallCount, 6U);
    state = r.Read();
    ASSERT_EQ(state, TStreamMsgReader::TState::AtEnd);
    ASSERT_EQ(r.OnGetNextMsg.CallCount, 7U);
    ASSERT_EQ(r.OnGetNextMsg.State, TStreamMsgReader::TState::ReadNeeded);
    ASSERT_EQ(r.OnGetNextMsg.Data, "");
    ASSERT_TRUE(r.OnGetNextMsg.EndOfInput);

    /* verify that there are 0 bytes of left over data */
    ASSERT_EQ(MakeDataStr(r), "");
    ASSERT_EQ(r.GetDataSize(), 0U);
  }

  TEST_F(TStreamMsgReaderTest, Test3) {
    /* test behavior with nonblocking file descriptor */
    TPipe p(MakePipe(true));
    TTestReader r(p.Read);

    /* create some message data for the reader to read */
    WritePipe(p.Write, "aa");

    /* reader should try to read next 20 bytes, get 2 bytes, and find no ready
       message */
    r.GetNextReadSizeReturnValue = 20;
    r.SetNoMsgReady();

    /* verify expected Read() behavior */
    ASSERT_EQ(r.OnGetNextMsg.CallCount, 0U);
    auto state = r.Read();
    ASSERT_EQ(state, TStreamMsgReader::TState::ReadNeeded);
    ASSERT_EQ(r.OnGetNextMsg.CallCount, 1U);
    ASSERT_EQ(r.OnGetNextMsg.State, TStreamMsgReader::TState::ReadNeeded);
    ASSERT_EQ(r.OnGetNextMsg.Data, "aa");
    ASSERT_FALSE(r.OnGetNextMsg.EndOfInput);

    /* reader should try again to read 20 bytes, get 0 bytes without blocking,
       and find no ready message */
    r.GetNextReadSizeReturnValue = 20;
    r.SetNoMsgReady();

    /* verify expected Read() behavior */
    ASSERT_EQ(r.OnGetNextMsg.CallCount, 1U);
    state = r.Read();
    ASSERT_EQ(state, TStreamMsgReader::TState::ReadNeeded);
    ASSERT_EQ(r.OnGetNextMsg.CallCount, 1U);

    /* write some more data and close write end of pipe */
    WritePipe(p.Write, "bbb");
    p.CloseWrite();

    /* reader should try again to read 20 bytes, get 3 bytes, and find a ready
       message 3 bytes long with a 2 byte header and no trailer */
    r.GetNextReadSizeReturnValue = 20;
    r.SetMsgReady(2, 3, 0);

    /* verify expected Read() behavior */
    ASSERT_EQ(r.OnGetNextMsg.CallCount, 1U);
    state = r.Read();
    ASSERT_EQ(state, TStreamMsgReader::TState::MsgReady);
    ASSERT_EQ(r.OnGetNextMsg.CallCount, 2U);
    ASSERT_EQ(r.OnGetNextMsg.State, TStreamMsgReader::TState::ReadNeeded);
    ASSERT_EQ(r.OnGetNextMsg.Data, "aabbb");
    ASSERT_FALSE(r.OnGetNextMsg.EndOfInput);

    /* mark message as consumed and verify expected state in
       BeforeConsumeReadyMsg() */
    r.SetNoMsgReady();
    ASSERT_EQ(r.OnBeforeConsumeReadyMsg.CallCount, 0U);
    ASSERT_EQ(r.OnGetNextMsg.CallCount, 2U);
    state = r.ConsumeReadyMsg();
    ASSERT_EQ(r.OnBeforeConsumeReadyMsg.CallCount, 1U);
    ASSERT_EQ(r.OnGetNextMsg.CallCount, 3U);
    ASSERT_EQ(state, TStreamMsgReader::TState::ReadNeeded);
    ASSERT_EQ(r.OnBeforeConsumeReadyMsg.State,
        TStreamMsgReader::TState::MsgReady);
    ASSERT_EQ(r.OnBeforeConsumeReadyMsg.Data, "aabbb");
    ASSERT_FALSE(r.OnBeforeConsumeReadyMsg.EndOfInput);
    ASSERT_EQ(r.ReadyStateOnBeforeConsumeReadyMsg.ReadyMsg, "bbb");
    ASSERT_EQ(r.ReadyStateOnBeforeConsumeReadyMsg.ReadyMsgOffset, 2U);
    ASSERT_EQ(r.OnGetNextMsg.State, TStreamMsgReader::TState::MsgReady);
    ASSERT_EQ(r.OnGetNextMsg.Data, "");
    ASSERT_FALSE(r.OnGetNextMsg.EndOfInput);

    /* reader should try again to read 20 bytes, get 0 bytes, detect end of
       input, and find no ready message */
    r.GetNextReadSizeReturnValue = 20;
    r.SetNoMsgReady();

    /* verify expected Read() behavior */
    ASSERT_EQ(r.OnGetNextMsg.CallCount, 3U);
    state = r.Read();
    ASSERT_EQ(state, TStreamMsgReader::TState::AtEnd);
    ASSERT_EQ(r.OnGetNextMsg.CallCount, 4U);
    ASSERT_EQ(r.OnGetNextMsg.State, TStreamMsgReader::TState::ReadNeeded);
    ASSERT_EQ(r.OnGetNextMsg.Data, "");
    ASSERT_TRUE(r.OnGetNextMsg.EndOfInput);

    /* verify that there are 0 bytes of left over data */
    ASSERT_EQ(MakeDataStr(r), "");
    ASSERT_EQ(r.GetDataSize(), 0U);
  }

  TEST_F(TStreamMsgReaderTest, Test4) {
    /* test behavior with error on read */
    TPipe p(MakePipe());
    TTestReader r(p.Read);

    /* close read end of pipe so reader will get error on attempted read */
    p.CloseRead();

    /* reader should try to read 20 bytes */
    r.GetNextReadSizeReturnValue = 20;
    r.SetNoMsgReady();

    bool caught = false;

    try {
      r.Read();  // should throw std::system_error
    } catch (const std::system_error &) {
      caught = true;
    } catch (...) {
      ASSERT_TRUE(false);
    }

    ASSERT_TRUE(caught);
  }

  TEST_F(TStreamMsgReaderTest, Test5) {
    /* test behavior with nonblocking file descriptor */
    TPipe p(MakePipe(true));
    TTestReader r(p.Read);

    /* create some message data for the reader to read */
    WritePipe(p.Write, "aa");

    /* reader should try to read next 20 bytes, and find invalid data */
    r.GetNextReadSizeReturnValue = 20;
    r.SetInvalid();

    /* verify expected Read() behavior */
    ASSERT_EQ(r.OnGetNextMsg.CallCount, 0U);
    auto state = r.Read();
    ASSERT_EQ(state, TStreamMsgReader::TState::DataInvalid);
    ASSERT_EQ(r.OnGetNextMsg.CallCount, 1U);
    ASSERT_EQ(r.OnGetNextMsg.State, TStreamMsgReader::TState::ReadNeeded);
    ASSERT_EQ(r.OnGetNextMsg.Data, "aa");
    ASSERT_FALSE(r.OnGetNextMsg.EndOfInput);
  }

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
