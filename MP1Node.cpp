    /**********************************
 * FILE NAME: MP1Node.cpp
 *
 * DESCRIPTION: Membership protocol run by this Node.
 * 				Definition of MP1Node class functions.
 **********************************/

#include "MP1Node.h"

/*
 * Note: You can change/add any functions in MP1Node.{h,cpp}
 */

/**
 * Overloaded Constructor of the MP1Node class
 * You can add new members to the class if you think it
 * is necessary for your logic to work
 */
MP1Node::MP1Node(Member *member, Params *params, EmulNet *emul, Log *log, Address *address) {
    for( int i = 0; i < 6; i++ ) {
        NULLADDR[i] = 0;
    }
    this->memberNode = member;
    this->emulNet = emul;
    this->log = log;
    this->par = params;
    this->memberNode->addr = *address;
}

/**
 * Destructor of the MP1Node class
 */
MP1Node::~MP1Node() {}

/**
 * FUNCTION NAME: recvLoop
 *
 * DESCRIPTION: This function receives message from the network and pushes into the queue
 * 				This function is called by a node to receive messages currently waiting for it
 */
int MP1Node::recvLoop() {
    if ( memberNode->bFailed ) {
        return false;
    }
    else {
        return emulNet->ENrecv(&(memberNode->addr), enqueueWrapper, NULL, 1, &(memberNode->mp1q));
    }
}

/**
 * FUNCTION NAME: enqueueWrapper
 *
 * DESCRIPTION: Enqueue the message from Emulnet into the queue
 */
int MP1Node::enqueueWrapper(void *env, char *buff, int size) {
    Queue q;
    return q.enqueue((queue<q_elt> *)env, (void *)buff, size);
}

/**
 * FUNCTION NAME: nodeStart
 *
 * DESCRIPTION: This function bootstraps the node
 * 				All initializations routines for a member.
 * 				Called by the application layer.
 */
void MP1Node::nodeStart(char *servaddrstr, short servport) {
    Address joinaddr;
    joinaddr = getJoinAddress();

    // Self booting routines
    if( initThisNode(&joinaddr) == -1 ) {
#ifdef DEBUGLOG
        log->LOG(&memberNode->addr, "init_thisnode failed. Exit.");
#endif
        exit(1);
    }

    if( !introduceSelfToGroup(&joinaddr) ) {
        finishUpThisNode();
#ifdef DEBUGLOG
        log->LOG(&memberNode->addr, "Unable to join self to group. Exiting.");
#endif
        exit(1);
    }

    return;
}

/**
 * FUNCTION NAME: initThisNode
 *
 * DESCRIPTION: Find out who I am and start up
 */
int MP1Node::initThisNode(Address *joinaddr) {
	/*
	 * This function is partially implemented and may require changes
	 */
	//int id = *(int*)(&memberNode->addr.addr);
	//int port = *(short*)(&memberNode->addr.addr[4]);

	memberNode->bFailed = false;
	memberNode->inited = true;
	memberNode->inGroup = false;
    // node is up!
	memberNode->nnb = 0;
	memberNode->heartbeat = 0;
	memberNode->pingCounter = -1;
	memberNode->timeOutCounter = TFAIL;
    initMemberListTable(memberNode);

    return 0;
}

/**
 * FUNCTION NAME: introduceSelfToGroup
 *
 * DESCRIPTION: Join the distributed system
 */
int MP1Node::introduceSelfToGroup(Address *joinaddr) {
	MessageHdr *msg;
#ifdef DEBUGLOG
    static char s[1024];
#endif

    if ( 0 == memcmp((char *)&(memberNode->addr.addr), (char *)&(joinaddr->addr), sizeof(memberNode->addr.addr))) {
        // I am the group booter (first process to join the group). Boot up the group
#ifdef DEBUGLOG
        log->LOG(&memberNode->addr, "Starting up group...");
#endif
        memberNode->inGroup = true;
    }
    else {
        size_t msgsize = sizeof(MessageHdr) + sizeof(memberNode->addr.addr) + sizeof(long);
        msg = (MessageHdr *) malloc(msgsize * sizeof(char));

        // create JOINREQ message: format of data is {struct Address myaddr}
        msg->msgType = JOINREQ;
        memcpy((char *)(msg+1), memberNode->addr.addr, sizeof(memberNode->addr.addr));
        memcpy((char *)(msg+1) + sizeof(memberNode->addr.addr), &memberNode->heartbeat, sizeof(long));

#ifdef DEBUGLOG
        sprintf(s, "Trying to join...");
        log->LOG(&memberNode->addr, s);
#endif
        // send JOINREQ message to introducer member
        emulNet->ENsend(&memberNode->addr, joinaddr, (char *)msg, msgsize);

        free(msg);
    }

    return 1;
}

/**
 * FUNCTION NAME: finishUpThisNode
 *
 * DESCRIPTION: Wind up this node and clean up state
 */
int MP1Node::finishUpThisNode(){
   /*
    * Your code goes here
    */
    memberNode->bFailed = true;
    memberNode->inGroup = false;

    return 0;
}

/**
 * FUNCTION NAME: nodeLoop
 *
 * DESCRIPTION: Executed periodically at each member
 * 				Check your messages in queue and perform membership protocol duties
 */
void MP1Node::nodeLoop() {
    if (memberNode->bFailed) {
    	return;
    }

    // Check my messages
    checkMessages();

    // Wait until you're in the group...
    if( !memberNode->inGroup ) {
    	return;
    }
    
    // ...then jump in and share your responsibilites! send messages!
    nodeLoopOps();

    return;
}

/**
 * FUNCTION NAME: checkMessages
 *
 * DESCRIPTION: Check messages in the queue and call the respective message handler
 */
void MP1Node::checkMessages() {
    void *ptr;
    int size;

    // Pop waiting messages from memberNode's mp1q
    while ( !memberNode->mp1q.empty() ) {
        ptr = memberNode->mp1q.front().elt;
        size = memberNode->mp1q.front().size;
        memberNode->mp1q.pop();
        recvCallBack((void *)memberNode, (char *)ptr, size);
    }
    return;
}

/**
 * FUNCTION NAME: recvCallBack
 *
 * DESCRIPTION: Message handler for different message types
 */
bool MP1Node::recvCallBack(void *env, char *data, int size ) {
	/*
	 * Your code goes here
	 */
    //structure of data[(MessageHdr)header,(Address)sender,(long)heartbeat]
    MessageHdr* msg = (MessageHdr*)data;
    Address addr;
    long heartbeat;
    
    switch(msg->msgType){
        case JOINREQ:
        {    
            memcpy(addr.addr,(char*)(msg+1),sizeof(addr.addr));
            memcpy(&heartbeat,(char*)(msg+1)+sizeof(addr.addr),sizeof(long));

            updateMemberList(&addr,heartbeat);
            sendMemberList(&addr);
            break;
        }

        case JOINREP:
        {
            int count;
            memberNode->inGroup = true;
            memberNode->timeOutCounter = TFAIL;
            size_t offset = sizeof(MessageHdr);
            
            memcpy(&count, (char*)(msg)+offset, sizeof(int));
            offset += sizeof(int);
            
            for(int i=0;i<count;i++){
                memcpy((int*)&addr.addr[0], (char*)(msg)+offset, sizeof(int));
                offset += sizeof(int);

                memcpy((short*)&addr.addr[4], (char*)(msg)+offset, sizeof(short));
                offset += sizeof(short);

                memcpy(&heartbeat, (char*)(msg)+offset, sizeof(long));
                offset += sizeof(long);

                updateMemberList(&addr,heartbeat);
            }
            break;
        }
        default:
            return false;
    }
    free(msg);
    return true;
}

/**
 * FUNCTION NAME: nodeLoopOps
 *
 * DESCRIPTION: Check if any node hasn't responded within a timeout period and then delete
 * 				the nodes
 * 				Propagate your membership list
 */
void MP1Node::nodeLoopOps() {
	/*
	 * Your code goes here
	 */
    memberNode->myPos = memberNode->memberList.begin();
    memberNode->myPos->timestamp++;
    memberNode->timeOutCounter--;
    memberNode->heartbeat++;
    memberNode->myPos->setheartbeat(memberNode->heartbeat);
    //this node failure
    if(memberNode->timeOutCounter==0){
        finishUpThisNode();
        return;
    }

    //detect failure
    vector<MemberListEntry>::iterator i;
    for(i=memberNode->memberList.begin()+1;i!=memberNode->memberList.end();){
        //for an entry, if heartbeat==-1, consider it fails but not removed
        if(i->gettimestamp() + TREMOVE + TFAIL < memberNode->myPos->gettimestamp()){
            Address addr;
            *(int*)(&addr.addr[0]) = i->getid();
            *(short*)(&addr.addr[4]) = i->getport();
            log->logNodeRemove(&memberNode->addr,&addr);
            i = memberNode->memberList.erase(i);
        }
        else{
            if(i->gettimestamp() + TFAIL < memberNode->myPos->gettimestamp())
                i->setheartbeat(-1);
            i++;   
        }
    }

    //propagate my memberlist to except myself
    for(i=memberNode->memberList.begin()+1;i!=memberNode->memberList.end();i++){
        if(i->getheartbeat()>=0){
            Address addr;
            *(int*)(&addr.addr[0]) = i->getid();
            *(short*)(&addr.addr[4]) = i->getport();
            sendMemberList(&addr);
        }    
    }
    return;
}

/**
 * FUNCTION NAME: isNullAddress
 *
 * DESCRIPTION: Function checks if the address is NULL
 */
int MP1Node::isNullAddress(Address *addr) {
    return (memcmp(addr->addr, NULLADDR, 6) == 0 ? 1 : 0);
}

/**
 * FUNCTION NAME: getJoinAddress
 *
 * DESCRIPTION: Returns the Address of the coordinator
 */
Address MP1Node::getJoinAddress() {
    Address joinaddr;

    memset(&joinaddr, 0, sizeof(Address));
    *(int *)(&joinaddr.addr) = 1;
    *(short *)(&joinaddr.addr[4]) = 0;

    return joinaddr;
}

/**
 * FUNCTION NAME: initMemberListTable
 *
 * DESCRIPTION: Initialize the membership list
 */
void MP1Node::initMemberListTable(Member *memberNode) {
	memberNode->memberList.clear();
    int id = *(int*)(&memberNode->addr.addr[0]);
    int port = *(short*)(&memberNode->addr.addr[4]);
    MemberListEntry entry = MemberListEntry(id, port, 0, 0);
    memberNode->memberList.push_back(entry);
    //log->logNodeAdd(&memberNode->addr,&memberNode->addr);
}

/**
 * FUNCTION NAME: updataMemberList
 *
 * DESCRIPTION: Update the membership list
 */
void MP1Node::updateMemberList(Address* addr, long heartbeat) {
    /*
     * Your code goes here
     */  
    //printAddress(addr);

    int id = *(int *)(&addr->addr[0]);
    short port = *(short *)(&addr->addr[4]);
    memberNode->myPos = memberNode->memberList.begin();
    if(id==memberNode->myPos->id && port==memberNode->myPos->port) return;

    vector<MemberListEntry>::iterator it;

    for(it = memberNode->memberList.begin()+1;it!=memberNode->memberList.end();it++){
        if(it->getid()==id && it->getport()==port){
            if(heartbeat==-1 || it->getheartbeat()==-1){
                it->setheartbeat(-1);
                return;
            }

            if(heartbeat > it->getheartbeat()){
                it->setheartbeat(heartbeat);
                it->settimestamp(memberNode->myPos->gettimestamp()); 
            } 
            return; 
        } 
    } 

    //if no such entry
    MemberListEntry entry = MemberListEntry(id, port, heartbeat, memberNode->myPos->gettimestamp());
    memberNode->memberList.push_back(entry);
    log->logNodeAdd(&memberNode->addr,addr);
}

/**
 * FUNCTION NAME: sendMemberList
 *
 * DESCRIPTION: Send the membership list to other nodes
 */
void MP1Node::sendMemberList(Address* addr) {
    /*
     * Your code goes here
     */
    int count = memberNode->memberList.size();

    size_t msgsize = sizeof(MessageHdr) + sizeof(int)
                        + count * ( sizeof(int) + sizeof(short) + sizeof(long));
    size_t offset = 0;

    MessageHdr *msg = (MessageHdr*) malloc(msgsize * sizeof(char));

    offset += sizeof(MessageHdr);
    
    msg->msgType = JOINREP;

    memcpy((char*)(msg)+offset, &count, sizeof(int));
    offset += sizeof(int);

    vector<MemberListEntry>::iterator i;
    for(i=memberNode->memberList.begin();i!=memberNode->memberList.end();++i){
        memcpy((char*)(msg)+offset, &i->id, sizeof(int));
        offset += sizeof(int);

        memcpy((char*)(msg)+offset, &i->port, sizeof(short));
        offset += sizeof(short);

        memcpy((char*)(msg)+offset, &i->heartbeat, sizeof(long));
        offset += sizeof(long);
    }

    emulNet->ENsend(&memberNode->addr, addr, (char*)msg, msgsize);

    return;
}

/**
 * FUNCTION NAME: printAddress
 *
 * DESCRIPTION: Print the Address
 */
void MP1Node::printAddress(Address *addr)
{
    printf("%d.%d.%d.%d:%d \n",  addr->addr[0],addr->addr[1],addr->addr[2],
                                                       addr->addr[3], *(short*)&addr->addr[4]) ;    
}
