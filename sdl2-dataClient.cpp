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
#include <thread>
#include <chrono>

using namespace Poco::Net;
using namespace Poco;
using namespace std;

static double steering = 0;
static double power1 = 0;
static double power2 = 0;
static double power3 = 0;
string message;

double mapRangeSteering(int x) {
    return -50 + ((double)(x + 32768) * 100 / 65535);
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
        << "}";
    return jsonStream.str();
}

WebSocket* connectWebSocket() {
    while (true) {
        try {
            HTTPClientSession session("127.0.0.1", 8000);
            HTTPRequest request(HTTPRequest::HTTP_GET, "/", HTTPMessage::HTTP_1_1);
            HTTPResponse response;

            WebSocket* ws = new WebSocket(session, request, response);
            cout << "WebSocket connected." << endl;

            string hello = "Hello, Server!";
            ws->sendFrame(hello.data(), hello.size(), WebSocket::FRAME_TEXT);
            return ws;
        }
        catch (Poco::Exception& ex) {
            cerr << "WebSocket connection failed: " << ex.displayText() << " - retrying in 3s..." << endl;
            this_thread::sleep_for(chrono::seconds(3));
        }
    }
}

int main(int argc, char* argv[]) {
    WebSocket* ws = nullptr;
    SDL_Joystick* joystick = nullptr;

    if (SDL_Init(SDL_INIT_JOYSTICK | SDL_INIT_EVENTS) < 0) {
        cerr << "SDL couldn't start: " << SDL_GetError() << endl;
        return -1;
    }

    SDL_JoystickEventState(SDL_ENABLE);
    ws = connectWebSocket();

    SDL_Event event;
    bool running = true;
    bool inputStarted = false;
    auto lastSendTime = chrono::steady_clock::now();

    while (running) {
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_JOYDEVICEADDED:
                if (!joystick) {
                    joystick = SDL_JoystickOpen(event.jdevice.which);
                    if (joystick) {
                        cout << "Joystick connected: " << SDL_JoystickName(joystick) << endl;
                    }
                }
                break;

            case SDL_JOYDEVICEREMOVED:
                if (joystick) {
                    cout << "Joystick disconnected." << endl;
                    SDL_JoystickClose(joystick);
                    joystick = nullptr;
                }
                break;

            case SDL_JOYAXISMOTION:
                if (joystick) {
                    try {
                        if (!inputStarted) {
                            inputStarted = true;
                        }
                        switch (event.jaxis.axis) {
                        case 0:
                            steering = mapRangeSteering((int)event.jaxis.value);
                            message = createMessage(steering, power1, power2, power3);
                            ws->sendFrame(message.data(), message.size(), WebSocket::FRAME_TEXT);
                            break;
                        case 2:
                            power1 = mapRangePower((int)event.jaxis.value);
                            message = createMessage(steering, power1, power2, power3);
                            ws->sendFrame(message.data(), message.size(), WebSocket::FRAME_TEXT);
                            break;
                        case 3:
                            power2 = mapRangePower((int)event.jaxis.value);
                            message = createMessage(steering, power1, power2, power3);
                            ws->sendFrame(message.data(), message.size(), WebSocket::FRAME_TEXT);
                            break;
                        case 4:
                            power3 = mapRangePower((int)event.jaxis.value);
                            message = createMessage(steering, power1, power2, power3);
                            ws->sendFrame(message.data(), message.size(), WebSocket::FRAME_TEXT);
                            break;
                        }
                    }
                    catch (Poco::Exception& ex) {
                        cerr << "WebSocket send failed (message dropped): " << ex.displayText() << endl;

                        try {
                            if (ws) {
                                ws->shutdown();
                                delete ws;
                                ws = nullptr;
                            }
                        }
                        catch (...) {}

                        ws = connectWebSocket();
                    }

                }
                break;
            }
        }
        if (joystick && inputStarted) {
            auto now = chrono::steady_clock::now();
            auto elapsed = chrono::duration_cast<chrono::milliseconds>(now - lastSendTime);

            if (elapsed.count() >= 3000) {
                try {
                    message = createMessage(steering, power1, power2, power3);
                    ws->sendFrame(message.data(), message.size(), WebSocket::FRAME_TEXT);
                    lastSendTime = now;
                }
                catch (Poco::Exception& ex) {
                    cerr << "WebSocket periodic send failed: " << ex.displayText() << endl;

                    try {
                        if (ws) {
                            ws->shutdown();
                            delete ws;
                            ws = nullptr;
                        }
                    }
                    catch (...) {}

                    ws = connectWebSocket();
                    lastSendTime = chrono::steady_clock::now();
                }
            }
            SDL_Delay(40);
	}
    }

    if (joystick) SDL_JoystickClose(joystick);
    SDL_Quit();

    if (ws) {
        try {
            ws->shutdown();
            delete ws;
        }
        catch (...) {
            cerr << "WebSocket shutdown error." << endl;
        }
    }

    return 0;
}
