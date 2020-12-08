----------------------------------------------

Local (Ubuntu) run using 128.113.0.2
Before:
MSS: 536 bytes
RECV Buffer Size: 131072 bytes
After:
MSS: 1460 bytes
RECV Buffer Size: 131072 bytes

----------------------------------------------

Lab 9 Submitty Test - Google Test using 108.177.112.106
Before:
MSS: 536 bytes
RECV Buffer Size: 87380 bytes
After:
MSS: 1380 bytes
RECV Buffer Size: 374400 bytes

----------------------------------------------

Lab 9 Submitty Test - RPI Test using 128.113.0.2
Before:
MSS: 536 bytes
RECV Buffer Size: 87380 bytes
After:
MSS: 1460 bytes
RECV Buffer Size: 374400 bytes

----------------------------------------------

Lab 9 Submitty Test - localhost Test using 127.0.0.1
Before:
MSS: 536 bytes
RECV Buffer Size: 87380 bytes
After:
MSS: 21845 bytes
RECV Buffer Size: 1062000 bytes

----------------------------------------------

Explanation:
The MSS is defaulted to octets of 536 bytes before connection. They negotiate the MSS value on both ends in the initial handshake, which is why the MSS changes after we connect to the given IP in each of the test cases.
The RECV buffers start with a default value when we created a socket. After connection is established, both sides negotiated their window scale which requires scaling their receive buffers to accomodate for larger window sizes they may have negotiated. That is why some of the recv buffers changed size after connection.
