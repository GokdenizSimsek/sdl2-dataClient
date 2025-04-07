#include <SDL2/SDL.h>
#include <Poco/Net/HTTPClientSession.h>
#include <Poco/Net/WebSocket.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/Exception.h>

#include <iostream>
#include <iomanip>
#include <string>
#include <sstream>

using namespace Poco::Net;
using namespace Poco;
using namespace std;

static double steering; // -50 to 50
static double power1; // 0 to 100
static double power2; // 0 to 100
static double power3; // 0 to 100

static string message = "";

double mapRangeSteering(int x) {
    return -50 + (double)(x + 32768) * 100 / 65535;
}

double mapRangePower(int x) {
    return 100 - (((x + 32768) / 65535.0) * 100);
}

std::string createMessage(double steering, double power1, double power2, double power3) {
    std::ostringstream jsonStream;
    jsonStream << std::fixed << std::setprecision(2);

    jsonStream << "{"
        << "\"Steering\": " << steering << ", "
        << "\"Power1\": " << power1 << ", "
        << "\"Power2\": " << power2 << ", "
        << "\"Power3\": " << power3
        << "}" << std::endl;

    return jsonStream.str();
}

int main(int argc, char* argv[]) {
    try {
        HTTPClientSession session("127.0.0.1", 8000);
        HTTPRequest request(HTTPRequest::HTTP_GET, "/", HTTPMessage::HTTP_1_1);
        HTTPResponse response;

        WebSocket ws(session, request, response);
        cout << "Connected to WebSocket server!" << endl;

        message = "Hello, Server!";
        ws.sendFrame(message.data(), message.size(), WebSocket::FRAME_TEXT);
        
        
        // Initialize SDL
        if (SDL_Init(SDL_INIT_JOYSTICK) < 0) {
            std::cerr << "SDL initialization failed! Error: " << SDL_GetError() << std::endl;
            return -1;
        }
        std::cout << "SDL initialized succesfully!" << std::endl;

        if (SDL_NumJoysticks() < 1) {
            std::cerr << "No connected joystick found!" << std::endl;
        }
        else {
            SDL_Joystick* joystick = SDL_JoystickOpen(0);
            if (joystick) {
                std::cout << "Joystick is Connected: " << SDL_JoystickName(joystick) << std::endl;
                bool isWorking = true;
                SDL_Event event;

                while (isWorking) {
                    while (SDL_PollEvent(&event)) {
                        if (event.type == SDL_JOYAXISMOTION) {
                            switch (event.jaxis.axis) {
                            case 0:
                                steering = mapRangeSteering((int)event.jaxis.value);
                                message = createMessage(steering, power1, power2, power3);
                                ws.sendFrame(message.data(), message.size(), WebSocket::FRAME_TEXT);
                                break;
                            case 2:
                                power1 = mapRangePower((int)(event.jaxis.value));
                                message = createMessage(steering, power1, power2, power3);
                                ws.sendFrame(message.data(), message.size(), WebSocket::FRAME_TEXT);
                                break;
                            case 3:
                                power2 = mapRangePower((int)(event.jaxis.value));
                                message = createMessage(steering, power1, power2, power3);
                                ws.sendFrame(message.data(), message.size(), WebSocket::FRAME_TEXT);
                                break;
                            case 4:
                                power3 = mapRangePower((int)(event.jaxis.value));
                                message = createMessage(steering, power1, power2, power3);
                                ws.sendFrame(message.data(), message.size(), WebSocket::FRAME_TEXT);
                                break;
                            default:
                                break;
                            }
                        }
                    }
                    SDL_Delay(40);

                }
                SDL_JoystickClose(joystick);

            } else {
                cerr << "Joystick cannot connect!" << endl;
            }
            SDL_Quit();
	    }
		ws.shutdown();
		cout << "Disconnected from WebSocket server!" << endl;
    }
    catch (Poco::Exception& ex) {
        cerr << "WebSocket Error: " << ex.displayText() << endl;
    }
    return 0;
}