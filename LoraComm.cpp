#include "LoraComm.h"

LoraComm::LoraComm(SoftwareSerial *port,void (*onMessageCb)(int address, const char* msg, int snr, int rssi)) {
  m_port = port;
  m_onMessageCb = onMessageCb;
  m_onAckCb = NULL;
}

void LoraComm::setAckCb(void (*onAckCb)(int fromAddress, int msgId)) {
  m_onAckCb = onAckCb;
}

void LoraComm::initNetwork(int networkId, int address, char *cpin) {
  // send AT+NETWORDID=
  // send AT+ADDRESS=
  // send AT+CPIN=
}

void LoraComm::begin() {
  
}

void LoraComm::setVerbose(boolean verbose){
  
}

void LoraComm::setResend(byte maxRetries, int resendWaitMillis){
  
}

void LoraComm::sendATCommand(char *msg){
  m_port->print(msg);
  m_port->print("\r\n");

  Serial.print("sendATCommand:");
  Serial.println(msg);
}

void LoraComm::rawSend(int address, char *msg){
  sprintf(m_buf,"AT+SEND=%d,%d,%s",address,strlen(msg),msg);
  m_port->print(m_buf);
  m_port->print("\r\n");

  Serial.print("rawSend:");
  Serial.println(m_buf);
}

int LoraComm::send(int toAddress, const char *msg, boolean needsAck){
  m_outMessage = new LoraMessage(toAddress, msg, ++m_msgId, needsAck);
  rawSend(toAddress, m_outMessage->getOutBuffer());
  m_inMessage->resetMessage();
  if (needsAck) {
    m_waitingForAck = true;
    m_waitStart = millis(); 
    m_tries = 0;   
  }
  delay(700);
}

void LoraComm::parse(char *s) {
  for(int i=0;i<strlen(s);i++) {
    m_inMessage->addReceived(s[i]);
  }
  m_inMessage->addReceived('\r');
  loop();
}

void LoraComm::loop() {
  while(m_port->available()) {
    int c = m_port->read();
    m_inMessage->addReceived((char)c);    
  }

  if (m_inMessage->isAck() && m_inMessage->getAckMsgId()==m_outMessage->getMsgId()) {
    m_waitingForAck = false;
  }

  if (m_inMessage->isComplete() && m_inMessage->isAck()) {
    if (m_onAckCb) {
      m_onAckCb(m_inMessage->getAddress(), m_inMessage->getAckMsgId());
    }
  }
  
  if (m_inMessage->isComplete() && !m_inMessage->isAck()) {
    if (m_onMessageCb) {
      m_onMessageCb(m_inMessage->getAddress(), m_inMessage->getMessage(), m_inMessage->getSNR(), m_inMessage->getRSSI());
    }
  }

  if (m_waitingForAck && (millis()-m_waitStart > m_resendWaitMillis)) {
    // Resend the last message
    if (m_tries<m_maxRetries) {
      m_tries++;
      rawSend(m_outMessage->getAddress(), m_outMessage->getOutBuffer());
      m_waitStart=millis();
      m_inMessage->resetMessage();
    } else {
      m_waitingForAck = false;
      Serial.println("Error: waiting too long for ACK");
    }
  }
    // parse
        // +ERR=1
        // +ERR=2
        // +NETWORKID=nnnnnn
        // +ADDRESS=nnnnnn
        // +VER=
        // +UID=
        // +PARAMETER=a,b,c,d
        // +CPIN=
        // +RCV=3,7,ACK:123,12,-34
        // +RCV=<senderAddress>,<length>,<msg>,<rssi>,<snr>

    // if waiting for ACK and timeout 
    //   if m.tries < maxRetries:
    //      send it again
    //      m.tries++
    //      restart waitTimer
    //   else:
    //     log tries exceeded
    //     waiting = false
  
}

LoraMessage::LoraMessage() {
  strcpy(m_message,"");
  strcpy(m_msgType,"");
}

LoraMessage::LoraMessage(int toAddress, const char *msg, int msgId, boolean needsAck) {
  m_address = toAddress;
  m_needsAck = needsAck;
  m_msgId = msgId;
  sprintf(m_message, "%s,%d,", msg, msgId);

  uint8_t checksum = 0;
  for(int i=0;i<strlen(m_message);i++) {
    checksum+= m_message[i];
  }
  checksum = 0xFF - checksum;
  sprintf(m_message,"%s%d", m_message, checksum);
}

boolean LoraMessage::isAcked() {
  return m_isAcked;
}

int LoraMessage::getAckMsgId() {
  return m_ackMsgId;
}

boolean LoraMessage::isAck() {
  return m_isAck;
}

int LoraMessage::getSNR() {
  return m_snr;
}

int LoraMessage::getRSSI() {
  return m_rssi;
}

int LoraMessage::getAddress() {
  return m_address;
}

char* LoraMessage::getMessage() {
  return m_message;
}

boolean LoraMessage::isComplete() {
  return m_isComplete;
}

void LoraMessage::resetMessage() {
  m_isComplete = false;
  m_snr = 0;
  m_rssi = 0;
  m_address = 0;
  strcpy(m_message,"");
  strcpy(m_raw,"");
  strcpy(m_msgType,"");  
  m_isAck = false;
  m_needsAck = false;
  m_isAcked = false;
  m_msgId = -1;
  m_ackMsgId = 0;  
}

char* LoraMessage::getOutBuffer() {
  return m_message;  
}

int LoraMessage::getMsgId() {
  return m_msgId;
}

void LoraMessage::removeChars(char *str, byte s) {
  if (s>strlen(str)) {
    return;
  }

  byte its = strlen(str)-s;
  for(byte i=1;i<=its;i++) {
    str[i-1]=str[i-1+s];
    str[i-1+s]=0;
  }
}

boolean LoraMessage::startsWith(char *str, char *startWithWord) {

  if (strlen(str)<strlen(startWithWord)) {
    return false;
  }
  
  byte i=1;
  while(i<=strlen(startWithWord)) {
    if (str[i-1]!=startWithWord[i-1]) {
      return false;
    }
    i++;
  }
  return true;
}

void LoraMessage::parse(char *msg) {

  strcpy(m_raw,"");
  strcpy(m_message,"");
  
  for(int i=0;i<strlen(msg);i++) {
    addReceived(msg[i]);
  }
  addReceived('\r');
  Serial.println("After parse:");
  Serial.print("Msg:");
  Serial.println(m_message);
  Serial.print("Address:");
  Serial.print(m_address);
  Serial.print(" msgId:");
  Serial.print(m_msgId);
  Serial.print(" snr:");
  Serial.print(m_snr);
  Serial.print(" rssi:");
  Serial.println(m_rssi);
}

boolean LoraMessage::addReceived(char s) {
    
  if (s=='\r' || s=='\n') {
    // +RCV=<senderAddress>,<length>,<msg>,<rssi>,<snr>
    // <msg> = <realMsg>,<crc>
    strcpy(m_message,"");
    addCharToString(m_raw,',');
    Serial.print("Received:");
    Serial.println(m_raw);
    if (startsWith(m_raw,"+RCV=")) {      
      Serial.println("is RCV");      
      removeChars(m_raw,strlen("+RCV="));
      Serial.print("Removed RCV. Left:");
      Serial.println(m_raw);
      char wrk[100];
      char msg[100];
      strcpy(wrk,"");
      strcpy(msg,"");
      int msgLen = 0;
      byte kw = 0;  // 0 = address
                    // 1 = strlen
                    // 2 = msg
                    // 3 = snr
                    // 4 = rssi
      for(int i=0;i<strlen(m_raw);i++) {
        if (m_raw[i]==',') {
          if (kw==0) {
            // Now we have all the chars for the sender's address
            m_address = atoi(wrk);
            strcpy(wrk,"");
            kw++;
          } else if (kw==1) {
            // Now we have all the chars for the msgLength
            msgLen=atoi(wrk);
            strcpy(wrk,"");
            kw++;            
          } else if (kw==2) {
            // a character for the msg (could be a ',')
            if (strlen(msg)<msgLen ) {
              // keep reading until len
              addCharToString(msg,m_raw[i]);                    
            } else {
              // We read the whole msg. Lets parse it.
              parsePayload(msg);
              strcpy(msg,"");
              kw++;
            }
          } else if (kw==3) {
            // Now we have all the chars for the snr
            m_snr = atoi(wrk);
            strcpy(wrk,"");
            kw++;            
          } else if (kw==4) {
            // Now we have all the chars for the rssi
            m_rssi = atoi(wrk);
            strcpy(wrk,"");
            kw++;            
          }
        } else {
          if (kw==2) {
            addCharToString(msg,m_raw[i]);                      
          } else {
            addCharToString(wrk,m_raw[i]);
          }
        }
      }
      strcpy(m_raw,"");
    }
  } else {
    m_isComplete = false;
    if (strlen(m_raw)<100) {
      addCharToString(m_raw,s);
    } else {
      Serial.println("Too long!!!");
      strcpy(m_raw,"");
    }
  }
  return false;
}

void LoraMessage::parsePayload(char *s) {
  Serial.print("parse:");
  Serial.println(s);
  
  // <message>,<msgid>,<crc>
  int p = reverseFind(s,',');
  if (p==-1) {
    return;
  }
  char w[strlen(s)+5];
  strcpy(w,"");
  for(int i=p+1;i<strlen(s);i++) {
    addCharToString(w,s[i]);
  }
  Serial.print("String w:");
  Serial.println(w);
  uint8_t msgCrc = atoi(w);
  uint8_t crc = 0;
  s[p]=0; // chop off the crc
  Serial.print("String s:");
  Serial.println(s);  
  Serial.print("Adding:");
  for(int i=0;i<strlen(s);i++) {
    Serial.print(s[i]);
    Serial.print("+");
    crc+= s[i];
  }
  crc+= ','; // The crc was calculated on the <msg>,<msgid>, so we need that last ',' to be included in the crc
  crc = 0xFF - crc;
  Serial.print(" =");
  Serial.println(crc);
  if (crc==msgCrc) {
    Serial.println("crc same");    
  } else {
    Serial.println("crc not same");    
  }
  Serial.print("p=");
  Serial.print(p);
  Serial.print(" crc w=");
  Serial.print(w);
  Serial.print(" ");
  Serial.print(msgCrc);
  Serial.print(" ");
  Serial.println(crc);  

  if (crc==msgCrc) {
    strcpy(m_message,"");
    p = reverseFind(s,',');
    if (p==-1) {
      return;
    }
    strcpy(w,"");
    for(int i=p+1;i<strlen(s);i++) {
      addCharToString(w,s[i]);
    }
    m_msgId = atoi(w);
    Serial.print("msgId:");
    Serial.println(m_msgId);  
    s[p]=0; // strip off the msgId
    Serial.print("String s:");
    Serial.println(s);  
    for(int i=0;i<strlen(s);i++) {
      addCharToString(m_message,s[i]);
    }
    Serial.print("m_message:");
    Serial.println(m_message);    

    // Check if this was an ACK for a message.
    parseAck(m_message);
    m_isComplete = true;
  }
}

void LoraMessage::parseAck(char *s) {
  if (startsWith(s,"ACK:")) {
    removeChars(s,strlen("ACK:"));
    m_ackMsgId = atoi(s);
    m_isAck = true;
  }
}

int LoraMessage::reverseFind(char *s, char c) {

  // abcde,12,45
  // 01234567890
  // len=11
  
  for(int i=strlen(s);i>1;i--) {
    if (s[i-1]==c) {
      return i-1;
    }
  }
  return -1;
}

void LoraMessage::addCharToString(char *s, char c) {
  char t[2];
  t[0]=c;
  t[1]=0;
  strcat(s,t);             
}

void LoraMessage::init() {

}
