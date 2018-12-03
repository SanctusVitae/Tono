#ifndef _LOG_MESSAGE_H_
#define _LOG_MESSAGE_H_

#include <Arduino.h>

class LogMessage {
  // private by default
    int       m_Level = 0;
    const int LevelNone = -1;
    const int LevelLog = 0;
    const int LevelWarn = 1;
    const int LevelError = 2;
 public:
    void      MessageLog(String msg) { if (m_Level >= 0) { Serial.println(msg); } }
    void      MessageWarn(String msg) { if (m_Level >= 1) { Serial.println(msg); } }
    void      MessageError(String msg) { if (m_Level = 2) { Serial.println(msg); } }
    void      SetLevel(int level) { m_Level = level; }
};

class debugTimer {
 // private by default
 unsigned long          m_startTime;
 unsigned long          m_endTime;
 public:
        debugTimer()    { m_startTime = millis(); }
        ~debugTimer()   { m_endTime = millis(); m_endTime = m_endTime - m_startTime; Serial.print("Time elapsed :"); Serial.println(m_endTime); }
};

#endif
