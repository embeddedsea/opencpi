//#define DEBUG_TxRx_Datagram 1
/*
 * This file is protected by Copyright. Please refer to the COPYRIGHT file
 * distributed with this source distribution.
 *
 * This file is part of OpenCPI <http://www.opencpi.org>
 *
 * OpenCPI is free software: you can redistribute it and/or modify it under the
 * terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option) any
 * later version.
 *
 * OpenCPI is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
 * details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Abstract:
 *   This file contains the interface for the Ocpi Datagram transfer driver.
 *
 *  John Miller -  5-24-12
 *  Initial version
 *
 */

#include "fasttime.h"
#include "OcpiOsMisc.h"
#include "OcpiOsAssert.h"
#include "OcpiUtilMisc.h"
#include "DtDataGramXfer.h"

namespace XF = DataTransfer;
namespace OU = OCPI::Util;
namespace DataTransfer {
  namespace DDT = DtOsDataTypes;
  namespace Datagram {

const char *datagramsocket = "datagram-socket"; // name passed to inherited template class
static const unsigned MAX_TRANSACTION_HISTORY = 512;  // Max records per source
static const unsigned MAX_FRAME_HISTORY = 0xff; 

XferServices::
XferServices(XferFactory &driver, EndPoint &source, EndPoint &target)
  : XF::XferServices(driver, source, target),
    m_freeFrames(FRAME_SEQ_MASK+1), m_frameSeq(1), m_frameSeqRecord(MAX_FRAME_HISTORY+1),
    m_msgTransactionRecord(MAX_TRANSACTION_HISTORY)
{
    m_last_ack_send = fasttime_getticks(); // thanks valgrind...
}

XferFactory::
XferFactory(const char *a_name) throw ()
  : XF::XferFactory(a_name) {
  ocpiDebug("In DatagramXferFactory::DatagramXferFactory()");
}

XferFactory::
~XferFactory() {
  ocpiDebug("DatagramXferFactory::~DatagramXferFactory entered");
#if 0
  std::vector<SmemServices*>::iterator it;
  for ( it=m_smems.begin(); it!=m_smems.end(); it++ ) {
    (*it)->stop();
    (*it)->join();
  }
  m_smems.clear();
#endif
}

void XferRequest::
modify(DtOsDataTypes::Offset  */*new_offsets[]*/, DtOsDataTypes::Offset  */*old_offsets[]*/) {
  ocpiAssert("modify not inplemented"==0);
}

XferRequest::
~XferRequest() {
}

// Create a transfer request
XferRequest* XferRequest::
copy(DtOsDataTypes::Offset srcoffs, DtOsDataTypes::Offset dstoffs, size_t nbytes, XferRequest::Flags flags) {

#ifdef DEBUG_TxRx_Datagram
  printf("\n\n *** COPY to %d, len = %d\n", dstoffs, nbytes );
#endif

  if (flags & XferRequest::FlagTransfer) {
    if (!init()) {
      init(0);
      add(NULL, 0, 0);
    }
    // Finalize the transaction since we now have the flag transfer information
    // and we know how many messages were actually sent for the transaction,
    // essentially filling in the constant fields
    uint32_t flag = *(uint32_t*)parent().from().sMemServices().map(srcoffs, 0);
    Message *m = &m_messages[0];
    for (unsigned n = 0; n < m_nMessagesTx; n++, m++) {
      m->hdr.transactionId = m_tid; 
      m->hdr.numMsgsInTransaction = (uint16_t)(m_nMessagesTx == 1 ? 0 : m_nMessagesTx);
      m->hdr.flagAddr = OCPI_UTRUNCATE(uint32_t, dstoffs);
      m->hdr.flagValue = flag;
    }
    return this;
  }
    
  // For each transfer at this level we have a header and payload.  We may need to break it up into
  // multiple "messages".  Each message gets it own header so each one is self contained and can be
  // acted upon by the receiver without being dependent on previous messages (which could get lost).
  // We ask the underlying transmission layer for the max frame payload size.
  size_t maxpl =
    parent().maxPayloadSize() - (sizeof(MsgHeader) + sizeof(FrameHeader) + 8);
  if (!init())
    // conservative estimate, but it still might be exceeded
    init((nbytes + maxpl - 1)/maxpl + 1);
  size_t length;
  for (uint8_t *src = (uint8_t*)parent().from().sMemServices().map(srcoffs,0); nbytes > 0;
       nbytes -= length, src += length, dstoffs += OCPI_UTRUNCATE(DDT::Offset, length)) {
    length = nbytes > maxpl ? maxpl : nbytes;
    add(src, dstoffs, length);
  }						
  return this;
}

// Group data transfer requests
XF::XferRequest &XferRequest::
group(XF::XferRequest*xr) {
  return *xr; // FIXME: an error of some type?
}

XferServices::
~XferServices() {
  ocpiDebug("DatagramXferServices::~DatagramXferServices entered");
  // Note that members are destroyed before base classes,
  // so our frames are destroyed, and then our children (xferrequests and transactions)
  // which makes sense because frames refer to transactions
  lock();
}

void XferServices::
post(Frame & frame) {
  frame.send_time = fasttime_getticks();
  if (frame.msg_count)
    frame.frameHdr.flags |= FRAME_FLAG_HAS_MESSAGES;
  static_cast<SmemServices *>(&m_from.sMemServices())->send(frame);
  // If there is nothing to ack (no messages) in this frame, free it as soon as it is sent.
  // The "send" is required to take it and not queue it (or at least copy it).
  if (!frame.msg_count)
    frame.release();
}

Frame *XferServices::  
nextFreeFrame() {
  OCPI::Util::SelfAutoMutex guard ( this );
  uint16_t seq = m_frameSeq++;
  uint16_t mseq = seq & FRAME_SEQ_MASK;
  ocpiAssert( mseq < m_freeFrames.size() );
  Frame & f = m_freeFrames[mseq];
  ocpiAssert( f.is_free );
  f.frameHdr.frameSeq = seq;
  //printf("**** Our Frame Seq = %d\n", f.frameHdr.frameSeq );
  f.is_free = false;
  return &f;
}

// Here are frames that we sent that are being ACK'ed
void XferServices::
ack(unsigned count, unsigned start) {
  for ( unsigned n=start; n<(start+count); n++ ) {
    //      printf("ACKING frame id = %d\n", n);
    releaseFrame ( n );
  }
}

// This is the list of ACK's that we have to send
void XferServices::
addFrameAck(FrameHeader *hdr) {
  OCPI::Util::SelfAutoMutex guard ( this );
  m_acks.push_back( hdr->frameSeq );

  //#define ACK_NOW
#ifdef ACK_NOW
  size_t bytes_left;
  Frame & frame = getFrame( bytes_left );
  frame.frameHdr.ackOnly = 1;
  ss    post( frame );
#endif

}

void XferServices::
releaseFrame (unsigned seq) {	
  unsigned mseq = seq & FRAME_SEQ_MASK;
  Frame &f = m_freeFrames[mseq];
  if (!f.is_free && f.frameHdr.frameSeq == seq)
    m_freeFrames[mseq].release();
  else
    ocpiDebug("Received ack 0x%x when frame is %s with num 0x%x",
	      seq, f.is_free ? "free" : "busy", f.frameHdr.frameSeq);
}

void XferServices::
sendAcks(uint64_t time_now, uint64_t timeout) {
  if (m_acks.size() && ( time_now - m_last_ack_send ) > timeout ) {
    size_t bytes_left;
    Frame & frame = getFrame( bytes_left );
    post( frame );
  }
}

Frame &XferServices::
getFrame(size_t & bytes_left) {
  OCPI::Util::SelfAutoMutex guard ( this );
  Frame & frame = *nextFreeFrame(); 

  bytes_left = (int) maxPayloadSize();

  frame.frameHdr.destId = m_to.mailBox();
  frame.frameHdr.srcId =  m_from.mailBox();
  frame.frameHdr.flags = 0;
  frame.frameHdr.ACKCount = 0;
  frame.transaction = 0;
  frame.msg_count = 0;
  frame.msg_start = 0;
  frame.endpoint = &m_to;
  frame.iovlen = 0;

  // We will piggyback any pending acks here
  if ( m_acks.size() ) {
    m_last_ack_send = fasttime_getticks();
    frame.frameHdr.ACKStart = m_acks[0];
    frame.frameHdr.ACKCount++;
    m_acks.pop_front();
    for ( unsigned  y=0; y<m_acks.size(); y++ ) {
      if ( m_acks[y]  == (unsigned)(frame.frameHdr.ACKStart+1)) {
	m_acks.pop_front();
	y = 0;
	frame.frameHdr.ACKCount++;
	continue;
      }
      else {
	break;
      }
    }
  }

  // This is a two byte pad for compatibility with the 14 byte ethernet header.
  frame.iov[frame.iovlen].iov_base = (void*) &frame.frameHdr;
  frame.iov[frame.iovlen].iov_len = 2;
  frame.iovlen++;
  frame.iov[frame.iovlen].iov_base = (void*) &frame.frameHdr;
  frame.iov[frame.iovlen].iov_len = sizeof(frame.frameHdr);	
  bytes_left -= ((int)frame.iov[1].iov_len + 2);
  frame.iovlen++;
		
  return frame;
}

void XferRequest::
post() {
  size_t bytes_left;
  uint16_t msg = 0;
  Transaction & t = *this;
  t.m_nMessagesRx = 0;
  while ( msg < t.msgCount() ) {

    // calculate the next message size
    //      if ( (int)(sizeof(DatagramMsgHeader) + t.hdrPtr(msg)->dataLen) > bytes_left ) {
    //	bytes_left = 0;
    //      }

    Frame & frame = parent().getFrame( bytes_left );
    frame.transaction = &t;
    frame.msg_start = msg;	  

    // Stuff as many messages into the frame as we can
    while (bytes_left > 0 && msg < t.msgCount()) {
      size_t need = sizeof(MsgHeader) + ((t.hdrPtr(msg)->dataLen + 7) & ~7);
      if ( bytes_left < need) {
	ocpiAssert(msg > 0);
	// Need a new frame
	t.hdrPtr(msg-1)->nextMsg = false;
	break;
      }

      frame.iov[frame.iovlen].iov_base = (void*) t.hdrPtr(msg);
      frame.iov[frame.iovlen].iov_len = sizeof(MsgHeader);
      frame.iovlen++;
      frame.iov[frame.iovlen].iov_base = (void*) t.srcPtr(msg);
      frame.iov[frame.iovlen].iov_len = t.hdrPtr(msg)->dataLen;
      // Adjust the aligment so that the next header is on a 8 byte boundary
      frame.iov[frame.iovlen].iov_len = (frame.iov[frame.iovlen].iov_len + 7) & ~7; 
      frame.iovlen++;
      bytes_left = bytes_left - (frame.iov[frame.iovlen-2].iov_len +
				 frame.iov[frame.iovlen-1].iov_len);
      t.hdrPtr(msg)->nextMsg = true;
      msg++;
      frame.msg_count++;
    }
    t.hdrPtr(msg-1)->nextMsg = false;
    parent().post( frame );
    //      queFrame( frame );	  
  }	
}

volatile static uint32_t g_txId;
static uint32_t getNextId() {
  g_txId++;
  return g_txId;
}

void Transaction::
init(size_t nMsgs) {
  ocpiAssert( ! m_init );
  m_nMessagesTx = 0;
  m_messages.reserve(nMsgs ? nMsgs : 1);
  m_tid = getNextId();
  m_init = true;
}

// src == NULL means the message only carries the flag and the transaction has no other messages
void Transaction::
add(uint8_t *src, DtOsDataTypes::Offset dst_offset, size_t length) {
  if (m_nMessagesTx >= m_messages.size())
    m_messages.reserve( m_nMessagesTx + 10 );
  m_messages.resize(++m_nMessagesTx);
  Message &m = m_messages.back();

  m.hdr.dataAddr = (uint32_t)dst_offset;
  m.hdr.dataLen = (uint16_t)length;
  m.src_adr = src;

  // This might disappear since we could put disconnect in a frame flag
  m.hdr.type = MsgHeader::DATA;
  // This might disappear unless there is a use-case for its heuristic value
  m.hdr.msgSequence = (uint16_t)m_nMessagesTx;
}

#ifdef PKT_DEBUG
    struct sockaddr_in * in = (struct sockaddr_in *)&sad;
    int df = ((struct sockaddr_in *)&sad)->sin_family;
    int port = ntohs ( ((struct sockaddr_in *)&sad)->sin_port );
    char * a  = inet_ntoa ( ((struct sockaddr_in *)&sad)->sin_addr );
    struct in_addr  adrr  =  ((struct sockaddr_in *)&sad)->sin_addr;
#endif

  
XF::XferRequest::CompletionStatus 
XferRequest::getStatus() { 
  // We only need to check the completion status of the flag transfer since it is gated on the 
  // completion status of the data and meta-data transfers
  if (! complete())
    return XF::XferRequest::Pending;
  return XF::XferRequest::CompleteSuccess;
}

void Socket::
run() {
  try {
    while ( m_run ) {
      unsigned size =	maxPayloadSize();
      uint8_t buf[size];
      size_t offset;
      size_t n = receive(buf, offset);
      if (n == 0)
	continue; // zero is timeout
      // This causes a frame drop for testing
      //#define DROP_FRAME
#ifdef DROP_FRAME
      const char* env = getenv("OCPI_Datagram_DROP_FRAMES");
      if ( env != NULL ) 
	{
	  static int dropit=1;
	  static int dt = 300;
	  static int m = 678900;
	  if ( dt && (((++dropit)%m)==0) ) {
	    printf("\n\n\n DROP A PACKET FOR TESTING \n\n\n");
	    dt--;
	    m = 500000 + rand()%10000;
	    continue;
	  }
	}
#endif
      // Get the xfer service that handles this conversation
      FrameHeader *header = reinterpret_cast<FrameHeader*>(&buf[offset + 2]);
      XferServices *xfs = m_lep.xferServices(header->srcId);
      if (xfs)
	xfs->processFrame(header);
    }
  }
  catch (std::string &s) {
    ocpiBad("Exception in socket background thread: %s", s.c_str());
  } catch (...) {
    ocpiBad("Unknown exception in socket background thread");
  }
  ocpiInfo("Datagram socket receiver thread exiting");
}

Socket::
~Socket() {
  try {
    stop();
    join();
  }
  catch( ... ) {
  }
}

SmemServices::
~SmemServices () {
  ocpiDebug("DatagramSmemServices::~DatagramSmemServices entered");
  OU::SelfAutoMutex guard(this);
  try {
    stop();
    delete [] m_mem;
    // Thread already joined join();
  }
  catch( ... ) {
  }
}

void 
DGEndPoint::
run() {
  const uint64_t timeout = 200 * 1000 * 1000;  // timeout in mSec
  uint64_t time_now;

  while ( m_loop ) {
    // If we have not received an ACK after (timeout) send the frame again.
    {
      OU::SelfAutoMutex guard(this);
      for (unsigned n=0; n < m_xferServices.size(); n++)
	if (m_xferServices[n] != NULL) {
	  time_now = fasttime_getticks();
	  m_xferServices[n]->checkAcks(time_now, timeout);
	  OCPI::OS::sleep(2);
	  time_now = fasttime_getticks();
	  m_xferServices[n]->sendAcks(time_now, timeout/3);
	  OCPI::OS::sleep(2);
	}
    }
    OCPI::OS::sleep(2);
  }
};

void XferServices::    
checkAcks( uint64_t time, uint64_t time_out ) {    
  OCPI::Util::SelfAutoMutex guard ( this );
  for ( unsigned n=0; n<m_freeFrames.size(); n++ ) {
    if ( ! m_freeFrames[n].is_free ) {
      if ( ( time - m_freeFrames[n].send_time ) > time_out ) {

	// We are multi-threaded and this can happen if a post occurs after this
	// method id called
	if ( time > m_freeFrames[n].send_time ) {

	  m_freeFrames[n].resends++;	    

#ifdef LIMIT_RETRIES
	  if ( m_freeFrames[n].resends < 4 ) {
	    printf("***** &&&&& Found a dropped frame, re-posting \n");
	    post( m_freeFrames[n] );
	  }
	  ss    else {
	    ocpiAssert("Exceeded resend limit !!\n"==0);
	  }
#else
	  post( m_freeFrames[n] );
#endif
	}
      }
    }
  }
}

void XferServices::  
processFrame(FrameHeader * header) {
  OCPI::Util::SelfAutoMutex guard(this);
  MsgHeader *msg;

  // It is possible for the sender to duplicate frames by being too agressive with re-retries
  // Dont process dups. 
  if (  m_frameSeqRecord[ header->frameSeq & MAX_FRAME_HISTORY ].id == header->frameSeq ) {
      
    ocpiDebug("max history = %d, SID = %d, fqr size = %zd mask=%d, seq = %d this = %p", 
	      MAX_FRAME_HISTORY, header->srcId,  m_frameSeqRecord.size(), 
	      header->frameSeq&MAX_FRAME_HISTORY, header->frameSeq, this
	      );

    if (  ! m_frameSeqRecord[ header->frameSeq & MAX_FRAME_HISTORY ].acked  ) {
      ocpiAssert("programming error, cant have dup without ACK "==0);
    }	    

    ocpiDebug("********  Found a duplicate frame, Ignoring it !!");
    // Need to ACK the dup anyway
    m_frameSeqRecord[ header->frameSeq & MAX_FRAME_HISTORY ].acked = true;
    addFrameAck( header );
    return;

  } else
    m_frameSeqRecord[ header->frameSeq & MAX_FRAME_HISTORY ].id = header->frameSeq;

  // This frame contains ACK responses	       
  if ( header->ACKCount ) {
    //      printf("Acking from %d, count = %d\n", header->ACKStart, header->ACKCount );
    ack(  header->ACKCount, header->ACKStart );
  }
  if (!(header->flags & FRAME_FLAG_HAS_MESSAGES))
    return;

  // For our response
  m_frameSeqRecord[header->frameSeq & MAX_FRAME_HISTORY].acked = true;
  addFrameAck(header);

  msg = reinterpret_cast<MsgHeader*>(&header[1]);
  do {
    MsgTransactionRecord & fr =
      m_msgTransactionRecord[ (msg->transactionId & MAX_TRANSACTION_HISTORY) ];
    if ( ! fr.in_use ) {
      fr.in_use = true;
      fr.msgsProcessed = 0;
      m_frames_in_play++;
      ocpiAssert( m_frames_in_play < MAX_TRANSACTION_HISTORY );
    }
    fr.transactionId = msg->transactionId;
    fr.numMsgsInTransaction = msg->numMsgsInTransaction;	      

    if (msg->numMsgsInTransaction != 0) {
      ocpiDebug("Msg info -->  addr=%d len=%d tid=%d",
		msg->dataAddr, msg->dataLen, msg->transactionId );
      
      switch ( msg->type ) {

      case MsgHeader::DATA:
      case MsgHeader::METADATA:
	{

	  char* dptr =(char*)from().sMemServices().map(msg->dataAddr, msg->dataLen);	  
	  if ( msg->dataLen == 4 ) {

#ifdef DEBUG_TxRx_Datagram
	    printf(" FLAG -> Writing %d bytes to offset %d, tid=%d\n",  msg->dataLen, msg->dataAddr, msg->transactionId );
	    int * v = (int*)&msg[1];
	    printf("replacing flag value %d with %d\n", (int)*dptr, (int)*v);
#endif
	  }
	  else {
	    uint8_t *data = reinterpret_cast<uint8_t*>(&msg[1]);
	    memcpy(dptr, data, msg->dataLen);
	  }

	}
	break;

	// Not yet handled
      case MsgHeader::DISCONNECT:
	break;	    
	ocpiAssert("Unhandled Datagram message type"==0);
      }

      fr.msgsProcessed++;
    }
    // Close the transaction if needed
    if ( fr.msgsProcessed == fr.numMsgsInTransaction ) {
      char* dptr =(char*)m_from.sMemServices().map(msg->flagAddr, 4);	  

#ifdef DEBUG_TxRx_Datagram
      printf("**** Finalizing transaction %d, addr = %d, value = %d, old value=%d\n", 
	     msg->transactionId, msg->flagAddr,msg->flagValue, *dptr);
#endif

      memcpy(dptr, &msg->flagValue, 4);
      fr.in_use = false;
      m_frames_in_play--;
    }

    if  ( msg->nextMsg ) {

#ifdef DEBUG_TxRx_Datagram
      printf("Got a multi message frame !!\n");
#endif

      uint8_t * d = (uint8_t*)&msg[1];
      int len = (msg->dataLen +7) & ~7;
      d += len;
      msg = reinterpret_cast<MsgHeader*>(d);
    }
    else {
      msg = NULL;
    }

  } while ( msg );	    

}

void DGEndPoint::
addXfer(XferServices &s) {
  if (s.to().mailBox() >= m_xferServices.size())
    m_xferServices.resize(s.to().mailBox() + 8);
  m_xferServices[s.to().mailBox()] = &s;
  ocpiDebug("xfer service %p added with mbox %d", &s, s.to().mailBox());
}

void Frame::
release() {
  // This can occur if we are too agressive with a retry and end up getting back Two ACK's 
  // for the re-transmitted frame.
  if (is_free)
    return;
  // We have to release the individual messages for the transaction here.
  for (unsigned int n=msg_start; n<(msg_count+msg_start); n++ ) {
    ocpiAssert( transaction );
    transaction->ACK( n );
  }
  transaction = 0;
  is_free = true;
  resends = 0;
}
}
}
