# Cloud-Computing-PA1

In "All to All membership" branch, I realize an "All to All membership" protocol, it is much easier than gossip based protocol.

In "Gossip based protocol", I realize a pseudo-gossip protocol, that is, only randomly choose GOSSIPNUM of Nodes in local memberList to share my memberList. It may fail sometimes, however when GOSSIPNUM is set to more than 4, the whole system is stable. The reason is, I only share my memberList periodically, however, in real gossip method, when receiving new entry in memberList, the Node has to gossip to others immediately. In order to realize this, in my opinion, we need another message "Gossip". I think it is not so hard to do based on this code. 

Welcome discussion with me!
