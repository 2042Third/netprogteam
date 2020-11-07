#!/usr/bin/env python3

from concurrent import futures
import sys  # For sys.argv, sys.exit()
import socket  # for gethostbyname()

import grpc

import csci4220_hw3_pb2
import csci4220_hw3_pb2_grpc

from math import log, abs

# # A singly linked list that we can use for the LRU???
# class LList():
# 	def __init__(self, val=None, ptr = None):
# 		self.ptr = ptr # Points to the next LList object
# 		self.val = val

# 	# Returns the index/order rank of a certain value in the given linked list
# 	def getIndex(self, val):
# 		ind = -1
# 		nd = self
# 		while (nd.val != val and nd.ptr is not None):
# 			ind += 1
# 			nd = nd.ptr
# 		if (nd.val == val):
# 			return ind + 1
# 		else:
# 			return -1

class DHT():
	def __init__(self, k, ourNode):
		self.k = k          # What k is for k-buckets
		self.ourNode = ourNode  # Stores our csci4220_hw3_pb2.Node()
		self.N = 4          # Length of our ID is always 4 bits
		self.hashTable = [[] for i in range(0, self.N)] # stores the DHT as a list of list of csci4220_hw3_pb2.Node()
		  # Least recently seen is at the head, most recently seen is at the tail

	def kClosest(self, idkey):
		# idkey is the request/query key or id
		# Returns the k-closest nodes to {idkey} from all k-buckets in self.hashTable

		distance = idkey ^ self.ourNode.id
		allNodes = [n for row in self.hashTable for n in row] # Take all the nodes from the entire DHT
		# Sort them by distance to the requested ID  
		allNodes.sort(key = lambda x : x.id ^ idkey + abs(distance - x.id ^ self.ourNode.id))
		return allNodes[:self.k]

	def print(self):
		# Prints the DHT as specified by the PDF
		for i in range(0, self.N):
			print(f'{i}:', end=" ")
			for entry in self.hashTable[i]:
				print(f'{entry.id}:{entry.port}' end=" ")
			print()

	def __getitem__(self, key):
		if key >= len(self.hashTable):
			return None
		else:
			return self.hashTable[key]
	def __setitem__(self, key, val):
		if key >= 0 and len(self.hashTable) > key:
			self.hashTable[key] = val

	def updateNodeLoc(self, node):
		# Updates the given node to be the most recently used in our DHT
		# If it doesn't exist, it'll insert it according to the most recently used
		i = round(log(node.id ^ self.ourNode.id))
		pos = (self.DHT[i]).index(node)
		if pos == -1:
			# Doesn't exist, so insert it
			(self.DHT[i]).append(node)
			if len(self.DHT[i]) > self.k:
				(self.DHT[i]).pop(0)
		else: # Exists, so update it's location
			(self.DHT[i]).append((self.DHT[i]).pop(pos))

class KadImplServicer(csci4220_hw3_pb2_grpc.KadImplServicer):
	def __init__(self, k, ourNode):
		self.DHT = DHT(k, ourNode)
		self.values = {} # Dictionary for the values that maps key -> value

	def FindNode(self, request, context):
		"""Takes an ID (use shared IDKey message type) and returns k nodes with
		distance closest to ID requested
		"""
		# request is IDKey
		# We can ignore context
		print(f'Serving FindNode({request.idkey}) request for {request.node.id}', flush=True))
		# closestNodes = self.DHT[round(log(distance))] # At most k nodes in this

		kClosest = self.DHT.kClosest(request.idkey)

		# update the k-bucket of the requestor i.e. request.node.id
		self.DHT.updateNodeLoc(request.node)
		
		return csci4220_hw3_pb2.NodeList(responding_node = self.DHT.ourNode,
																		 nodes = kClosest)

	def FindValue(self, request, context):
		"""Complicated - we might get a value back, or we might get k nodes with
		distance closest to key requested
		"""
		print(f'Serving FindKey({request.idkey} request for {request.node.id}', flush=True)
		if self.values.get(request.idkey) is not None:
			# Found the key
			return csci4220_hw3_pb2.KV_Node_Wrapper(
				responding_node = self.DHT.ourNode,
				mode_kv = True,
				kv = self.value[request.idkey]
				nodes = []
			)
		else:
			# Exact same as FindNode
			kClosest = self.DHT.kClosest(request.idkey)

			# update the k-bucket of the requestor i.e. request.node.id
			self.DHT.updateNodeLoc(request.node)
			
			return csci4220_hw3_pb2.KV_Node_Wrapper(
				responding_node = self.DHT.ourNode,
				mode_kv = False,
				kv = None
				nodes = kClosest
			)

	def Store(self, request, context):
		"""Store the value at the given node. Need to return *something*, but client
		does not use this return value.
		"""
		print(f'Storing key {request.key} value "{request.value}"', flush=True)
		self.DHT.values[request.key] = request.value
		self.DHT.updateNodeLoc(request.node)

	def Quit(self, request, context):
		"""Notify a remote node that the node with ID in IDKey is quitting the
		network and should be removed from the remote node's k-buckets. Client
		does not use this return value.
		"""
		for i in range(0, self.DHT.N):
			if (request.id in self.DHT[i]):
				print(f'Evicting quitting node {request.id} from bucket {i}', flush=True)
				self.DHT[i].remove(request.node)
				return request
		print(f'No record of quitting node {request.id} in k-buckets.' flush=True)
		return request


def run():
	if len(sys.argv) != 4:
		print("Error, correct usage is {} [my id] [my port] [k]".format(sys.argv[0]))
		sys.exit(-1)

	local_id = int(sys.argv[1])
	my_port = str(int(sys.argv[2])) # add_insecure_port() will want a string
	k = int(sys.argv[3])
	my_hostname = socket.gethostname() # Gets my host name
	my_address = socket.gethostbyname(my_hostname) # Gets my IP address from my hostname

	''' Use the following code to convert a hostname to an IP and start a channel
	Note that every stub needs a channel attached to it
	When you are done with a channel you should call .close() on the channel.
	Submitty may kill your program if you have too many file descriptors open
	at the same time. '''
	
	#remote_addr = socket.gethostbyname(remote_addr_string)
	#remote_port = int(remote_port_string)
	#channel = grpc.insecure_channel(remote_addr + ':' + str(remote_port))

	ourNode = csci4220_hw3_pb2.Node(id = local_id, port = int(my_port), address = my_address)
	ourServicer = KadImplServicer(k, ourNode)

  # Code to start the servicer for our node
  server = grpc.server(futures.ThreadPoolExecutor(max_workers=10))
	csci4220_hw3_pb2_grpc.add_KadImplServicer_to_server(
			ourServicer, server)
	server.add_insecure_port(f'[::]:{my_port}')
	server.start() # Let servicer start listening w/t blocking

	while True:
		userInput = input() # Perhaps change this to select, but that's a discussion for later :P
		userInput = userInput.trim().split()
		if ((userInput[0]).lower().trim() == "bootstrap"):
			remoteHost = (userInput[1]).trim()
			remotePort = (userInput[2]).trim()
			remote_addr = socket.gethostbyname(remoteHost)
			remote_port = int(remotePort)
			with grpc.insecure_channel(remote_addr + ':' + str(remote_port)) as channel:
				stub = csci4220_hw3_pb2_grpc.KadImplStub(channel)
				result = stub.FindNode(csci4220_hw3_pb2.IDKey(node = ourNode, idkey = ourNode.id))
				# remoteID = [id for node in result if node.address == remote_addr and node.port == remote_port][0]
				remoteID = result.responding_node.id
				ourServicer.DHT.updateNodeLoc(result.responding_node)
				print(f'After BOOTSTRAP({remoteID}, k_buckets now look like:')
				ourServicer.DHT.print()

		elif ((userInput[0]).lower().trim() == "find_node" or (userInput[0]).lower().trim() == "find_value"):
			nodeID = int((userInput[1]).trim())
			findNode = (userInput[0]).lower().trim() == "find_node"
			print(f'Before FIND_{'NODE' if findNode else 'VALUE'} command, k-buckets are')
			ourServicer.DHT.print()

			found = nodeID == local_id
			value = (found, ourServicer.values.get(nodeID))

			if (not found):
				# Do the search
				Visited = {}
				nodeOfInterest = csci4220_hw3_pb2.IDKey(node = ourNode, idkey = nodeID)
				
				# Condition for us to use FindNode or FindValue
				S = ourServicer.DHT.kClosest(nodeOfInterest.idkey) # get k closest to idkey

				found = nodeID in [n.id for n in S]
				while not found and len(S) > 0:
					S = ourServicer.DHT.kClosest(nodeOfInterest.idkey) # get k closest nodes to idkey
					notVisited = [n for n in S if n.id not in Visited]
					for node in notVisited:
						with grpc.insecure_channel(node.address + ':' + str(node.port)) as channel:
							stub = csci4220_hw3_pb2_grpc.KadImplStub(channel)
							
							# Condition for us to use FindNode or FindValue
							R = stub.FindNode(nodeOfInterest) if findNode else stub.FindValue(nodeOfInterest)

							# Update our k-buckets with node
							ourServicer.DHT.updateNodeLoc(R.responding_node)
							
							if not findNode and R.mode_kv: # If we're for FindValue
								found = True
								value = (R.responding_node.id == ourNode.id, R.kv)
							
							# If a node in R was already in a k-bucket, its position does not change.
							# If it was not in the bucket yet, then it is added as the most recently used in that bucket.
							# This _may_ kick out the node from above.
							for n in R.nodes:
								if findNode and (n.id ^ nodeOfInterest.idKey) == 0:
									found = True
								ourServicer.DHT.updateNodeLoc(n)
							
							Visited.add(node)
							if found:
								break
					
			# Do the printing
			print(f'After FIND_{'NODE' if findNode else 'VALUE'} command, k-buckets are:')
			ourServicer.DHT.print()
			if (found):
				if findNode:
					print(f'Found destination id {nodeID}', flush=True)
				else:
					if value[0]:
						print(f'Found data "{value}" for key {nodeID}')
					else:
						print(f'Found value "{value}" for key {nodeID}')
			else:
				if findNode:
					print(f'Could not find destination id {nodeID}', flush=True)
				else:
					print(f'Could not find key {nodeID}')

		elif ((userInput[0]).lower().trim() == "store"):
			key = int(userInput[1].trim())
			val = userInput[2].trim()
			# Find the node it knows that's closest to the key
			closestNode = (ourServicer.DHT.kClosest(key))[0]
			if (ourNode.id ^ key <= closestNode.id ^ key):
				print(f'Storing key {key} at node {ourNode.id}', flush=True)
				ourServicer.DHT.values[key] = val
			else:
				print(f'Storing key {key} at node {closestNode.id}', flush=True)
				with grpc.insecure_channel(closestNode.address + ':' + str(closestNode.port)) as channel:
					stub = csci4220_hw3_pb2_grpc.KadImplStub(channel)
					stub.Store(csci4220_hw3_pb2.KeyValue(node = ourNode,
					                                     key = key
																							 value = val))

		elif ((userInput[0]).lower().trim() == "quit"):
			for row in ourServicer.DHT:
				for node in row:
					with grpc.insecure_channel(node.address + ':' + str(node.port)) as channel:
						stub = csci4220_hw3_pb2_grpc.KadImplStub(channel)
						print(f'Letting {node.id} know I\'m quitting.', flush=True)
						stub.Quit(csci4220_hw3_pb2.IDKey(node = ourNode,
						                                  idkey = ourNode.id))

			print(f'Shut down node {ourNode.id}', flush=True)
			server.stop()
			break
		else:
			print("Invalid Command", flush=True)


if __name__ == '__main__':
	run()
