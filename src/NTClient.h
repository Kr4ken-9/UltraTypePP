#include "httplib.h"
#include "json.hpp"
#include "utils.h"

#include <iostream>
#include <vector>
#include <string>
#include <uWS/uWS.h>
#include <map>
#include <algorithm>
#include <ctime>
using namespace std;
using namespace nlohmann;
using namespace uWS;

#define NITROTYPE_HOSTNAME "www.nitrotype.com"
#define NT_REALTIME_HOST "realtime1.nitrotype.com"
// wss://realtime1.nitrotype.com/realtime/?_primuscb=1498950532425-0&EIO=3&transport=websocket&sid=FGGxSq9DqbZyy7Z_AwlQ&t=1498950532723&b64=1
#define NT_REALTIME_WS_ENDPOINT "wss://realtime1.nitrotype.com/realtime/"
#define NT_PRIMUS_ENDPOINT "/realtime/"
#define NT_LOGIN_ENDPOINT "/api/login"
#define HTTP_PORT 80
#define HTTPS_PORT 443

class NTClient {
public:
	string uname;
	NTClient(string username, string password) {
		uname = username;
		pword = password;
		hasError = false;
		firstConnect = true;
	}
	bool login() {
		bool ret = true;
		string data = string("username=");
		data += uname;
		data += "&password=";
		data += pword;
		data += "&adb=1&tz=America%2FChicago"; // No need to have anything other than Chicago timezone
		httplib::SSLClient loginReq(NITROTYPE_HOSTNAME, HTTPS_PORT);
		shared_ptr<httplib::Response> res = loginReq.post(NT_LOGIN_ENDPOINT, data, "application/x-www-form-urlencoded; charset=UTF-8");
		if (res) {
			bool foundLoginCookie = false;
			for (int i = 0; i < res->cookies.size(); ++i) {
				string cookie = res->cookies.at(i);
				if (cookie.find("ntuserrem=") == 0) {
					foundLoginCookie = true;
					vector<string> parts = Utils::split(cookie, '=');
					string part1 = parts.at(1);
					vector<string> parts2 = Utils::split(part1, ';');
					token = parts2.at(0);
					cout << "Retrieved login token: " << token << endl;
				}
			}
			if (!foundLoginCookie) {
				ret = false;
				cout << "Unable to locate the login cookie. Maybe try a different account?\n";
			}
		} else {
			ret = false;
			cout << "Login request failed. This might be a network issue. Maybe try resetting your internet connection?\n";
		}
		bool success = getPrimusSID();
		if (!success) return false;
		return ret;
	}
	bool getPrimusSID() {
		time_t tnow = time(0);
		stringstream squery;
		squery << "?_primuscb=" << tnow << "-0&EIO=3&transport=polling&t=" << tnow << "-0&b64=1";
		string queryStr = squery.str();

		httplib::SSLClient loginReq(NT_REALTIME_HOST, HTTPS_PORT);
		string path = NT_PRIMUS_ENDPOINT + queryStr;
		loginCookie = "ntuserrem=" + token;
		shared_ptr<httplib::Response> res = loginReq.get(path.c_str(), loginCookie);
		if (res) {
			json jres = json::parse(res->body.substr(4, res->body.length()));
			primusSid = jres["sid"];
			cout << "Resolved primus SID: " << primusSid << endl;
		} else {
			cout << "Error retrieving primus handshake data.\n";
			return false;
		}
		return true;
	}
	void addListeners() {
		assert(ws == nullptr);
		ws->onError([this](void* udata) {
			cout << "Failed to connect to WebSocket server." << endl;
			hasError = true;
		});
		ws->onConnection([this](WebSocket<CLIENT>* wsocket, HttpRequest req) {
			cout << "Connected to the realtime server." << endl;
		});
		ws->onDisconnection([this](WebSocket<CLIENT>* wsocket, int code, char* msg, size_t len) {
			cout << "Disconnected from the realtime server." << endl;
		});
		ws->onMessage([this](WebSocket<SERVER>* ws, char* msg, size_t len, OpCode opCode) {
			cout << "ws message" << endl; // TODO: parse incoming messages
		});
	}
	bool connect() {
		ws = new Hub();
		time_t tnow = time(0);
		stringstream uristream;
		uristream  << NT_REALTIME_WS_ENDPOINT << "?_primuscb=" << tnow << "-0&EIO=3&transport=websocket&sid=" << primusSid << "&t=" << tnow << "-0&b64=1";
		string wsURI = uristream.str();
		cout << "Connecting to endpoint: " << wsURI << endl;
		if (firstConnect) {
			firstConnect = false;
			addListeners();
		}
		return true;
	}
protected:
	Hub* ws;
	string token; // Login token
	string loginCookie; // For outgoing requests that require authentication
	string pword;
	string primusSid;
	bool hasError;
	bool firstConnect;
};