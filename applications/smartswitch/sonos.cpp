#include "sonos.h"
#include "spark_http_client/http_client.h"


/*----------------------------------------------------------------------*/
/* Global variables */
/*----------------------------------------------------------------------*/



HTTPClient myHttpClient;

// response buffer
char buffer[1024];
char request[REQUEST_LEN];
char header[400];
char linebuffer[85];

char soapActionHeader[__SOAP_ACTION_HEADER_LENGTH];
char soapBody[__SOAP_BODY_LENGTH];

// maximum time the stored volume is accepted
// after time expired, volume will be fetched from the remote
// intention is to reduce request number especially when 
// user tries to increase/decrease the volume multiple times
// the propability that this method interferes with volume
// changes of other sources can be seen as quite small
#define __MAX_VOLUME_AGE 1500
short currentVolume = -1;
unsigned long lastVolumeRead = 0L;

short currentMute = 0;


static const uint16_t TIMEOUT = 5000; // Allow maximum 5s between data packets.

//#define SONOS_LOGING

byte sonosip[] = { 10, 0, 6, 118 };



SONOSClient::SONOSClient()
{
}


short SONOSClient::getMute() {
    return getShortValue("Mute", "CurrentMute");
}

void SONOSClient::setMute(bool muteit) {
  if (muteit) {
    setShortValue("Mute", 1);   
  } else {
    setShortValue("Mute", 0); 
  }
  
}


void SONOSClient::toggleMute() {
  currentMute = getMute();
  Serial.println(currentMute);
  
  if ( currentMute == 1) {
    Serial.println("unmute");
    setMute(FALSE); 
  } else {
    Serial.println("mute");
    setMute(TRUE);
  }
}


void SONOSClient::changeVolume(short delta) {
  if (delta != 0 && delta <= 100 && delta >= -100) {
    
    // negative value indicates error or unset state
    if (currentVolume == -1 || (millis()-lastVolumeRead)>__MAX_VOLUME_AGE) {
      #ifdef SERIAL_DEBUG
      Serial.println("Updating current Volume");
      #endif /* SERIAL_DEBUG */
      currentVolume = getVolume();
      lastVolumeRead = millis();
    } else {
      Serial.println("Using the stored Volume");
    }
    
    #ifdef SERIAL_DEBUG
    Serial.print("Current Volume: ");
    Serial.println(currentVolume);
    #endif /* SERIAL_DEBUG */
    
    currentVolume = currentVolume + delta;
    
    // upper limit
    if (currentVolume > 100) {
      currentVolume = 100;
    }

    // lower limit
    if (currentVolume < 0) {
      currentVolume = 0;
    }
    
    setVolume(currentVolume);
  }
}

short SONOSClient::getVolume() {
    return getShortValue("Volume", "CurrentVolume");
}

void SONOSClient::setVolume(short newVolume) {
  setShortValue("Volume", newVolume); 
 
}

/*----------------------------------------------------------------------*/
/* Private Methods */


/*----------------------------------------------------------------------*/
/* simple parsing of the responses                                      */
/* find <element>X</element> and return X as a String                   */
String SONOSClient::getXMLElementContent(String input, String element)
{
    // make open and close string
    String elementOpen = "<" + element + ">";
    String elementClose = "</" + element + ">";
    
    // get positions of open and close
    int openPos = input.indexOf(elementOpen);
    int closePos = input.indexOf(elementClose);
    
    // verify if open and close can be found in the input
    if (openPos == -1 || closePos == -1) {
		Serial.println("[ERR] can not find element in input");
		return NULL;
	}
    
    // idx: starts at index of element + length of element + tags
    int idx = openPos + elementOpen.length();
    
    // check if length is above 0
    if (closePos - idx > 0) {
        #ifdef SERIALDEBUGPARSER
            Serial.print("[INF] Output length: ");
            Serial.println((input.substring(idx,  closePos)).length());
        #endif
        return input.substring(idx,  closePos);

    } else {
        #ifdef SERIALDEBUGPARSER
            Serial.println("[ERR] idx seems wrong");
        #endif
        return NULL;
    }
}


/*----------------------------------------------------------------------*/
// soap set and get

// call like 
// setShortValue("Mute", 0)
void SONOSClient::setShortValue(const char *service, short valueToSet) 
{
  // create the soap action header field
  memset(soapActionHeader, 0, __SOAP_ACTION_HEADER_LENGTH);
  snprintf(soapActionHeader, __SOAP_ACTION_HEADER_LENGTH, "SOAPACTION: \"%s#Set%s",__SOAP_METHOD, service); 
    
  // create the soap body
  memset(soapBody, 0, __SOAP_BODY_LENGTH);
  snprintf(soapBody, __SOAP_BODY_LENGTH, "%s<u:Set%s xmlns:u=\"%s\">%s<Desired%s>%d</Desired%s></u:Set%s>%s", __SOAP_OPEN, service, __SOAP_METHOD, __SOAP_CHANNEL, service, valueToSet, service, service, __SOAP_CLOSE); /*content*/
  
  myHttpClient.makeRequest(1, /* type, 1 = POST */
                    "/MediaRenderer/RenderingControl/Control", /* URL */
                    sonosip, /* host */
                    __SONOS_PORT,  /* port */
                    FALSE, /* KEEP ALIVE */ 
                    "text/xml; charset=\"utf-8\"", /* contentType */
                    soapActionHeader, 
                    "", /* userHeader2 */ 
                    soapBody, /* content */
                    buffer, /* response */
                    1023, /* response size */
                    FALSE /* storeResponseHeader */
                      );
}

// returns integer value or -1 in case of an error
short SONOSClient::getShortValue(const char *service, const char *responsefield)
{
  // create the soap action header field
  memset(soapActionHeader, 0, __SOAP_ACTION_HEADER_LENGTH);
  snprintf(soapActionHeader, __SOAP_ACTION_HEADER_LENGTH, "SOAPACTION: \"%s#Get%s",__SOAP_METHOD, service); 
  
  // create the soap body
  memset(soapBody, 0, __SOAP_BODY_LENGTH);
  snprintf(soapBody, __SOAP_BODY_LENGTH, "%s<u:Get%s xmlns:u=\"%s\">%s</u:Get%s>\r\n", __SOAP_OPEN, service, __SOAP_METHOD, __SOAP_CHANNEL, service);
  
  
  myHttpClient.makeRequest(1, /* type, 1 = POST */
                    "/MediaRenderer/RenderingControl/Control", /* URL */
                    sonosip, /* host */
                    __SONOS_PORT,  /* port */
                    FALSE, /* KEEP ALIVE */ 
                    "text/xml; charset=\"utf-8\"", /* contentType */
                    soapActionHeader, 
                    "", /* userHeader2 */ 
                    soapBody, /*content*/
                    buffer, /* response */
                    1023, /* response size */
                    FALSE /* storeResponseHeader */
                      );

  if (strlen(buffer) > 0) {
    String returnedValue = getXMLElementContent(buffer, responsefield);
    if (returnedValue != NULL) {
      return returnedValue.toInt();
    }
  }

  // otherwise return -1 to indicate an error
  return -1;
  
}
