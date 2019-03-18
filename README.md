# Minikermit-File-Transfer-Simulator

This is a C language program, simulating the file transfer between two sockets, one is the input file, and one is the output file.
The transmission is a typical minikermit transmission, using packages of a specific size. A package is actually a buffer, and the program works with a 250-byte buffer size.
The program structure is the following:
-> the are two entities:
  -> the sender (ksender)
  -> the receiver (kreceiver)
-> the ksender's tasks are:
  -> sending the content of multiple files to the kreceiver. It is necesarry that it divides the content into packages that can be transfered between the two sockets, of a fixed size of 250 bytes
  -> the send operation is blocking, meaning that the sender waits for the receivers acknowledgement before sending another package
    -> if the kreceiver notifies the ksender with an ACK signal, the ksender transmits the next package
    -> else, in case of a NAK signal, or in case of timeout (which is actually set at 5 seconds), it transmits the same package as before
    -> there is a code sequence for each package. Transmitting the next package always increments the sequence with 1 unit.
-> the kreceiver's tasks are:
  -> verifying the CRC (Cyclic Redundance Check) of each package received.
    -> notifying back the ksender whether the package is corrupted or not (the CRC is correct, or not). In the latter case, the kreceiver waits the package with the same sequence number. Else, it is waiting for the package with the next sequence code.
    -> appending the package buffer content to the already existing content, if the content received is correct (not currupted)
