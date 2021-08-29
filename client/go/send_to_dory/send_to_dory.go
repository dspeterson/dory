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

   This is an example Go program that uses the doryclient library to send a
   message to Dory.
 */

package main

import (
    "flag"
    "fmt"
    "io/ioutil"
    "math"
    "net"
    "os"
    "path/filepath"
    "time"

    "github.com/dspeterson/dory/client/go/doryclient"
)

type socketType string

const (
    unixDgram socketType = "unix-dgram"
    unixStream socketType = "unix-stream"
)

type msgType string

const (
    anyPartition msgType = "AnyPartition"
    partitionKey msgType = "PartitionKey"
)

type cmdLineArgs struct {
    // type of socket to use for communication with Dory
    sockType socketType

    // path for Dory's socket file
    sockPath string

    // type of message to send (see Dory documentation)
    mType msgType

    // If mType above specifies partitionKey, use this as the partition key.
    pKey uint32

    // Kafka topic
    topic string

    // Kafka message key (can be empty)
    key string

    // Kafka message body
    value string
}

// Process and return command line args.  Return nil if args are invalid.
func getArgs() *cmdLineArgs {
    ret := &cmdLineArgs{}
    sockType := flag.String("socket-type", "unix-dgram",
        "unix-dgram or unix-stream")
    sockPath := flag.String("socket-path", "", "path to Dory's socket file")
    mType := flag.String("msg-type", "AnyPartition",
        "AnyPartition or PartitionKey")
    pKey := flag.Uint64("partition-key", 0,
        "partition key to use when sending PartitionKey message")
    topic := flag.String("topic", "", "Kafka topic")
    key := flag.String("key", "", "Kafka message key (can be empty)")
    value := flag.String("value", "", "Kafka message body")
    flag.Parse()

    if *sockType != string(unixDgram) && *sockType != string(unixStream) {
        _, _ = fmt.Fprintf(os.Stderr, "-socket-type must specify %v or %v\n",
            unixDgram, unixStream)
        return nil
    }

    if *sockPath == "" {
        _, _ = fmt.Fprintf(os.Stderr,
            "-socket-path argument is missing\n")
        return nil
    }

    if *mType != string(anyPartition) && *mType != string(partitionKey) {
        _, _ = fmt.Fprintf(os.Stderr, "-msg-type must specify %v or %v\n",
            anyPartition, partitionKey)
        return nil
    }

    if *pKey > math.MaxUint32 {
        _, _ = fmt.Fprintf(os.Stderr,
            "-partition-key value %v is too large: max is %v\n", *pKey,
            math.MaxUint32)
        return nil
    }

    if *topic == "" {
        _, _ = fmt.Fprintf(os.Stderr,
            "-topic argument is missing\n")
        return nil
    }

    ret.sockType = socketType(*sockType)
    ret.sockPath = *sockPath
    ret.mType = msgType(*mType)
    ret.pKey = uint32(*pKey)
    ret.topic = *topic
    ret.key = *key
    ret.value = *value
    return ret
}

func getEpochMilliseconds() uint64 {
    return uint64(time.Now().UnixNano() / int64(time.Millisecond))
}

// Send message to Dory as specified by args.  Return true on success or false
// on failure.
func sendMsg(args *cmdLineArgs) bool {
    var sockType string

    switch args.sockType {
    case unixDgram:
        sockType = "unixgram"  // UNIX domain datagram socket
    case unixStream:
        sockType = "unix"  // UNIX domain stream socket
    default:
        _, _ = fmt.Fprintf(os.Stderr,
            "internal error: unknown socket type [%v]\n", args.sockType)
        return false
    }

    topic := []byte(args.topic)  // Kafka topic
    msgKey := []byte(args.key)  // Kafka message key (may be empty)
    msgValue := []byte(args.value)  // Kafka message body

    // Create temporary directory for client socket file.
    dir, err := ioutil.TempDir("", "dory_go_client")
    if err != nil {
        _, _ = fmt.Fprintf(os.Stderr,
            "failed to create temp directory for client socket file: %v\n",
            err)
        return false
    }
    defer func() {
        cleanupErr := os.RemoveAll(dir)
        if cleanupErr != nil {
            _, _ = fmt.Fprintf(os.Stderr,
                "failed to remove temp directory for client socket file: %v\n",
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
        Name: args.sockPath,
        Net:  sockType,
    }
    conn, err := net.DialUnix(sockType, &clientAddr, &serverAddr)
    if err != nil {
        _, _ = fmt.Fprintf(os.Stderr, "failed to create %v socket: %v\n",
            sockType, err)
        return false
    }

    now := getEpochMilliseconds()
    var msg []byte

    switch args.mType {
    case anyPartition:
        msg, err = doryclient.CreateAnyPartitionMsg(topic, now, msgKey,
            msgValue)
        if err != nil {
            _, _ = fmt.Fprintf(os.Stderr,
                "failed to create AnyPartition message: %v\n", err)
            return false
        }
    case partitionKey:
        msg, err = doryclient.CreatePartitionKeyMsg(args.pKey, topic, now,
            msgKey, msgValue)
        if err != nil {
            _, _ = fmt.Fprintf(os.Stderr,
                "failed to create PartitionKey message: %v\n", err)
            return false
        }
    default:
        _, _ = fmt.Fprintf(os.Stderr,
            "internal error: unknown message type [%v]\n", args.mType)
        return false
    }

    _, err = conn.Write(msg)  // send to Dory
    if err != nil {
        _, _ = fmt.Fprintf(os.Stderr,
            "failed to send %v %v message: %v\n", args.mType, args.sockType,
            err)
        return false
    }

    return true
}

func main() {
    exitCode := 0
    defer func() { os.Exit(exitCode) }()

    args := getArgs()
    if args == nil {
        // invalid command line args
        exitCode = 1
        return
    }

    // Send message to Dory as specified by command line args.
    ok := sendMsg(args)
    if !ok {
        exitCode = 1
    }
}
