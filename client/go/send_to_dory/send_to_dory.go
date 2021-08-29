/* ----------------------------------------------------------------------------
   Copyright 2021 Dave Peterson <dave@dspeterson.com>

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

   This is an example Go program that uses the doryclient library to send
   messages to dory.
 */

package main

import (
    "fmt"
    "io/ioutil"
    "net"
    "os"
    "path/filepath"
    "send_to_dory/doryclient"
    "time"
)

func getEpochMilliseconds() uint64 {
    return uint64(time.Now().UnixNano() / int64(time.Millisecond))
}

// Example code for sending to Dory.  If useStreamSock is true, use UNIX domain
// stream sockets.  Otherwise use UNIX domain datagram sockets.  Return true on
// success or false on error.
func sendExampleMsgs(useStreamSock bool) bool {
    var doryPath string
    var sockType string

    if useStreamSock {
        doryPath = "/path/to/dory/stream_socket"
        sockType = "unix"
    } else {
        doryPath = "/path/to/dory/datagram_socket"
        sockType = "unixgram"
    }

    topic := []byte("some topic")  // Kafka topic
    msgKey := []byte("")
    msgValue := []byte("hello world")
    partitionKey := uint32(12345)

    // Create temporary directory for client socket file.
    dir, err := ioutil.TempDir("", "dory_go_client")
    if err != nil {
        _, _ = fmt.Fprintf(os.Stderr,
            "failed to create temp directory for client socket file: %v", err)
        return false
    }
    defer func() {
        cleanupErr := os.RemoveAll(dir)
        if cleanupErr != nil {
            _, _ = fmt.Fprintf(os.Stderr,
                "failed to remove temp directory for client socket file: %v",
                err)
        }
    }()

    // Set up socket communication with Dory.
    clientPath := filepath.Join(dir, "client.sock")
    clientAddr := net.UnixAddr{
        Name: clientPath,
        Net: sockType,
    }
    serverAddr := net.UnixAddr{
        Name: doryPath,
        Net:  sockType,
    }
    conn, err := net.DialUnix(sockType, &clientAddr, &serverAddr)
    if err != nil {
        _, _ = fmt.Fprintf(os.Stderr, "failed to create %v socket: %v",
            sockType, err)
        return false
    }

    // Create AnyPartition message.
    anyPartitionMsg, err := doryclient.CreateAnyPartitionMsg(topic,
        getEpochMilliseconds(), msgKey, msgValue)
    if err != nil {
        _, _ = fmt.Fprintf(os.Stderr,
            "failed to create AnyPartition message: %v", err)
        return false
    }

    // Send AnyPartition message to Dory.
    _, err = conn.Write(anyPartitionMsg)
    if err != nil {
        _, _ = fmt.Fprintf(os.Stderr,
            "failed to send AnyPartition %v message: %v", sockType, err)
        return false
    }

    // Create PartitionKey message.
    partitionKeyMsg, err := doryclient.CreatePartitionKeyMsg(partitionKey,
        topic, getEpochMilliseconds(), msgKey, msgValue)
    if err != nil {
        _, _ = fmt.Fprintf(os.Stderr,
            "failed to create PartitionKey message: %v", err)
        return false
    }

    // Send PartitionKey message to Dory.
    _, err = conn.Write(partitionKeyMsg)
    if err != nil {
        _, _ = fmt.Fprintf(os.Stderr,
            "failed to send PartitionKey %v message: %v", sockType, err)
        return false
    }

    return true
}

func main() {
    exitCode := 0
    defer func() { os.Exit(exitCode) }()

    // Send messages to Dory using UNIX domain datagram sockets.  To instead
    // use UNIX domain stream sockets, change the argument value to true.
    ok := sendExampleMsgs(false /* useStreamSock */)
    if !ok {
        exitCode = 1
    }
}
