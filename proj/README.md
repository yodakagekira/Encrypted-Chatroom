``` mermaid
flowchart TD
    Client["Client App"] --> Socket["TCP Socket"]
    Socket --> FrameIO["Frame I/O (read header + payload)"]

    subgraph ServerRuntime ["Server Runtime"]
        Listen["Listen Socket"] --> Accept["accept4() loop"]
        Accept --> EpollReg["epoll_ctl(ADD fd)"]
        EpollReg --> Wait["epoll_wait()"]
        Wait --> Events["FD Events: EPOLLIN / EPOLLOUT / ERR"]
    end

    Events --> Dispatcher["Event Dispatcher"]

    subgraph PerConnection ["Per-Connection Pipeline"]
        Dispatcher --> ReadPath["Read Path"]
        Dispatcher --> WritePath["Write Path"]

        ReadPath --> RxBuf["RX Buffer (accumulate TCP stream)"]
        RxBuf --> ParseHdr["Parse FrameHeader (version/type/len)"]
        ParseHdr --> Bounds["Bounds Check (MAX_PAYLOAD)"]
        Bounds --> Decode["decode_message()"]

        Decode --> Crypto["CryptoContext"]
        Crypto --> Verify["Verify HMAC + Replay Check"]
        Verify --> Decrypt["AES-CBC Decrypt"]
        Decrypt --> Plain["Plaintext"]

        Plain --> Handle["Handle MessageType"]
        Handle --> Hub["ChatHub"]
        Hub --> Rooms["RoomManager"]
        Hub --> Route["Resolve Recipients"]

        Route --> Enqueue["queue_message(dst)"]
        Enqueue --> Encode["encode_message()"]
        Encode --> CryptoTx["CryptoContext (encrypt+mac)"]
        CryptoTx --> TxBuf["TX Buffer (frames queued)"]
    end

    WritePath --> Flush["flush_out_buffer(send)"]
    Flush --> Socket
    TxBuf --> Flush

```
