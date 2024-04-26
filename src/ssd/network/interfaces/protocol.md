# Message

Messages travel between server and client and optimizing read of those is an
important task to consider and implement. To combat issues with this, developed
protocol is versatile and quite comprehensive.

## General idea

Messages are formed in two ways: raw and structured. Raw messages support message type,
message size and raw data chunk. They are efficient in terms of data transfer and following
read/write operations, but are barely usable apart from transferring sound or simple commands.
Structured messages are different because payload is a protocol buffer, being efficient in transfer
of stream settings and other payload that is a bit more complicated.

## Protocol

Protocol has the following structure:

| ID | Size | Payload |
|----|------|---------|
|10 b| 8 b  | Size (B)|

ID consists of 7 bits - message type, 1 bit - type (raw, protobuf) and 2 bits are protocol version.