Ubus RPC Bus
------------

The ubus rpc bus is meant to be a peer to peer environment between one or more
clients where clients can publish objects and invoke remote methods on objects
published by other clients. 

Two main topologies are to be supported: star and p2p. 

In the star configuration several clients connect to a single client that will
act as a mediator for the clients that are connected to it. It's job is then to
make objects published by other clients available to everyone else. 

In the p2p configuration clients are connected directly to one or more other
clients and directly get notified of new objects published on any one of these
clients.  

Ubus consists of several libraries: 
	
	- blobpack: library for handling and packing binary blobs
	- json-c: for encoding and decoding json data
	- libusys: for event loop implementation
	- libubus2: the ubus messaging system implementation
	- ubus: the ubus command line client
	- ubusd: the ubus server

Ubus Protocol Messages
----------------------

INVOKE: 
	
Sent by client when client wants to invoke a method on another client. When
server receives the invoke message it looks in it's internal table of
connections for client with that id and then forwards the message to that
client. When server receives this message it will automatically fill in source
as the name of the connection that made the request.

	UBUS_MSG_INVOKE: [ path, method, [ data ] ]

REPLY: 

Sent by a client in reply to a call. The reply must have serial field set to
the same value as in the request message. Serial can not be zero and if reply
can not be matched with a request then it should be silently discarded. When
server receives a reply, it simply forwards the message to the destination.

Status is set to one of the ubus status values. Data contains a blob of the
reply data. It is by default a sub-array (instead of a table) in order to
support simple replies that only contain values without requiring software to
return key/value pairs.  

	UBUS_MSG_REPLY: [ data ]

ERROR: 
	
This is sent as response to a command if there was an error. 

	UBUS_MSG_ERROR: [ error_code, [ data ] ]

SIGNAL: 

An asynchronous signal sent from one peer to another peer or to all peers (if
destination is set to 0). 

	UBUS_MSG_SIGNAL: [ type, [ data ] ]

Wire Protocol
-------------

Peers connect to eachother using ubus socket abstraction. This socket abstracts
away the transport details and exposes a peer oriented message passing
interface. The ubus socket is responsible for packaging up messages into frames
which can be received by another peer either over UDP or TCP (this
specification does not care about lower level transport details). 

Ubus socket abstraction must provide means to do following: 
- send arbitrary sized messages to peers identified by a unique random integer. 
- connect to other peers
- accept connections from other peers if configured to listen for incoming connections. 
- provide means to set callback for receiving messages
- provide means to detect when new peer is connected and when a peer leaves. 

All peers on the socket layer are to be identified by a unique random 32 bit
integer that identifies that particular connection on current socket. The
socket does not make destinction between outgoing and incoming connections and
instead treats all connections as peers. 

Message Framing
---------------

UBUS frames consist of a ubus header and variable sized data. A frame can be
any size because frames do not need to arrive in one message. A frame is
transmitted (usually using TCP or through domain socket) as a stream to the
other peer. The other peer is responsible for parsing out message header and
then expect number of bytes to follow which is specified in the header. 

UBUS frame header: 

	 u8     u8    u16   u32    u32
	+-----------------------------------+
	| hs | type | seq | peer | data_len | 
	+-----------------------------------

All integers should be big endian (network byte order) format in the header. 

	hs: ubus header size
		should be sizeof(struct ubus_msg_header) always. It serves as an easy
		way to determine validity of the coming frame.  
	type: 
		ubus message type 
	seq: 
		sequence number. will be same for matching request and reply and should
		be increased each time a new request is sent. Handled by the
		application. 
	peer: 
		target peer where this frame should be sent. If 0 then frame is
		addressed to the socket that has received it. Otherwise the application
		should forward the frame to the peer specified by this field. 
	data_len: 
		length of the data that follows the frame header. 

Message Data
------------

Data is packed using binary blob format. It should not matter which format is
used for the binary blob inside the header as long as it can be parsed as a
structured data type with following essential structures: 
	
	*int8: single byte integer
	*int16: 2 byte int
	int32: 4 byte int
	*int64: 8 byte int
	string: a null terminated string
	float32: 32 bit float
	*float64: 64 bit float 
	array: a list of arbitrary values
	table: a list of key/value pairs
	*binary: application defined binary data
	bool: should be represented by an int8

	*) not supported when converting blob -> json -> blob

Default implementation currently packs messages using rather simple blobpack
format using libblobpack. A more advanced format of msgpack exists which
results in almost half as long messages but has not been implemented yet. 

It should be possible to convert blobs to and from json. In this case, integers
must be 32 bit long and binary data should be encoded as a base64 string. Other
types can be supported, but only if the data is accompanied by a corresponding
schema that states which types should be used for each data value. This is
possible for function arguments, but is more difficult to enforce for arbitrary
data.  

All messages should be enclosed into a root element of type "array" (list of
values). If it is necessary to send a table, this can be done by simply placing
the table inside this array. Root element of array type allows for highest
level of flexibility. 

Example message (translated to json): 

	[ 123, "string", { "table_field": "value" } ]

Peer Protocol
-------------

When peers connect to eachother, both ends should send eachother a list of
objects that their applications have exported locally. The default
configuration is peer-to-peer. It does not care about whether clients connect
to you or you connect to another client - the principles of passing messages
are the same. You can just assume that at any given time you are connected to x
number of clients who each can be connected to other clients. You can only
however talk directly to at most the client that is connected to a client you
also are connected to. The core protocol does not allow you to communicate
through larger number of hops - although such an abstraction could be
implemented in application code if it is necessary. 

	Step 1: peers A and B connect
		A sends a signal to B that it wants to be called "alice" 
		B checks if it can grant known name alice to A and if it is possible then B local assigns name alice to connection id that goes to A. 
		B sends a signal to A telling that it wants to be called "bob" and A goes through the same process of assigning known name to B 
	Step 2: C connects to B 
		.. the same process as above is repeated for B and C
		B sends a signal to A telling it that it has a client called charlie that has local id X on B. 
		A saves this information for resolving requests going from A to C later
	Step 3: A wants to do a remote call to an object on C 
		A looks up peer name charlie on bob using the local database  
		A sends a message of type METHOD_CALL to B specifying "peer" field in the header to be id of Charlie (instead of 0)
		B receives the frame with non-zero peer field, replaces peer with 0 and sets return peer to local id of A on B.
		B forwards the new frame to Charlie. 
	Step 4: C receives METHOD_CALL from B
		C receives a message from B with peer set to 0 and source peer set to address of A on B. 
		C handles the request and prepares reply with peer set to address of A on B 
		C sends the frame to B which then forwards it to A using the same scheme as above. 
	Step 5: A receives reply and resolves the local request
		A receives frame with peer set to 0 and type of METHOD_RETURN (or ERROR). 
		A looks up matching request based on "sequence" field of the message
		A resolves the request calling the user callback and deletes the request object. 

Built In Signals 
----------------

	ubus.object.add: 
		[ "ubus.object.add", path, [ signature ] ]

		sent by clients when a local object has been published. 

	ubus.object.delete: 
		[ "ubus.object.delete", path ]
		
		sent by clients when local object is deleted. 

	ubus.peer.well_known_name: 
		[ "ubus.peer.well_known_name", name ]

		sent by client to announce a name it would like to claim on current
		connection. If name can not be claimed on the receiving client then
		this signal should be ignored. 

Bus Protocol 
------------

When clients are connected to a bus client, additional protocol is required on
top of ubus to enable forwarding of messages between connected clients. 


