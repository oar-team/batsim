import struct
import socket
import sys
import os

server_address = '/tmp/bat_socket'

# Make sure the socket does not already exist
try:
    os.unlink(server_address)
except OSError:
    if os.path.exists(server_address):
        raise

sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)

# Bind the socket to the port
print >>sys.stderr, 'starting up on %s' % server_address
sock.bind(server_address)

# Listen for incoming connections
sock.listen(1)

while True:
    # Wait for a connection
    print >>sys.stderr, 'waiting for a connection'
    connection, client_address = sock.accept()
    try:
        print >>sys.stderr, 'connection from', client_address

        # Receive the data in small chunks and retransmit it
        while True:
            lg_str = connection.recv(4)
            print 'from client: %r' % lg_str
            lg = struct.unpack("i",lg_str)[0]
            print 'size msg to recv %d' % lg
            msg = connection.recv(lg)
            print 'from client: %r' % msg
            
            msg = "poypoy"
            lg = struct.pack("i",int(len(msg)))
            connection.sendall(lg)
            connection.sendall(msg)
            
            exit(1)

            #print >>sys.stderr, 'received %d' % data
            #if data:
            #    print >>sys.stderr, 'sending data back to the client'
            #    connection.sendall(data)
            #else:
            #    print >>sys.stderr, 'no more data from', client_address
            #    break
            
    finally:
        # Clean up the connection
        connection.close()
        
