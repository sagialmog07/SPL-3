#include <stdlib.h>
#include <thread>
#include <atomic>
#include <iostream>
#include "../include/ConnectionHandler.h"
#include "../include/StompProtocol.h"

// פונקציה לקריאת פקודות מהמקלדת ושליחתן לשרת
void keyboardThread(ConnectionHandler& connectionHandler, StompProtocol& protocol) {
    while (!protocol.shouldTerminate()) {
        const short bufsize = 1024;
        char buf[bufsize];
        
        if (!std::cin.getline(buf, bufsize)) {
            if (std::cin.eof()) {
                // אם המשתמש לחץ Ctrl+D, אנחנו פשוט יוצאים מהלולאה
                break;
            }
        }
        
        std::string line(buf);
        if (line.empty()) continue;

        std::string frame = protocol.processCommand(line);
        if (!frame.empty()) {
            if (!connectionHandler.sendFrameAscii(frame, '\0')) {
                break;
            }
        }
    }
}
int main(int argc, char *argv[]) {
    // אתחול רכיבי המערכת
    ConnectionHandler handler;
    StompProtocol protocol(handler);

    std::cout << "Waiting for commands (type 'login' to start)..." << std::endl;
    
    // Thread 1: מאזין ברקע לתשובות מהשרת ומעבד אותן
    std::thread listenerThread([&handler, &protocol]() {
        while (!protocol.shouldTerminate()) {
            std::string answer;
            // פונקציה חוסמת שמחכה להודעה מלאה שמסתיימת ב-'\0'
            if (handler.getFrameAscii(answer, '\0')) {
                // פיענוח הטקסט לאובייקט פריים וביצוע פעולות (כמו שמירת הודעות או אישור Receipts)
                StompFrame frame = protocol.parseRawFrame(answer);
                protocol.processFrame(frame);
            } else {
                // אם הסוקט נסגר או שיש שגיאת קריאה
                if (!protocol.shouldTerminate()) {
                    std::cout << "Connection lost." << std::endl;
                }
                break;
            }
        }
    });

    // Thread 2 (התהליכון הראשי): קריאת פקודות מהמקלדת
    keyboardThread(handler, protocol);
    
    // המתנה לסיום תהליכון ההאזנה (קורה אחרי logout מוצלח או שגיאה)
    if (listenerThread.joinable()) {
        listenerThread.join();
    }
    
    std::cout << "Client terminated." << std::endl;
    return 0;
}