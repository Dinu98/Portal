#include <pistache/net.h>
#include <pistache/http.h>
#include <pistache/peer.h>
#include <pistache/http_headers.h>
#include <pistache/router.h>
#include <pistache/endpoint.h>
#include <pistache/common.h>

#include <signal.h>
#include <algorithm>
#include <string>
#include <fstream>
//-lpaho-mqttpp3 -lpaho-mqtt3a
//#include <mqtt/client.h>

using namespace std;
using namespace Pistache;
using namespace Pistache::Rest;

#define INSIDE  "inside"
#define OUTSIDE "outside"
#define SHOCK   "isHit"
#define OM      "isValidHuman"
#define ANIMAL  "isValidAnimal"
#define APROAPE "inProximity"
#define INVITAT "hasPin"

Rest::Router router;
Http::Endpoint portal(Address(Ipv4::any(), Port(9080))); 

// "Portal" va avea mainDoor si petDoor
struct RFID_code {
    int type; // 1 pentru om si 2 pentru animal
	string name, code; 
};

// auth personal
RFID_code authPersonal[4];
string PIN = "00123";

void initAuthPersonal(){
	RFID_code authPers1, authPers2, dog1, cat1;
	
	authPers1.name = "Sotul";
	authPers1.code = "RFID9990001";
	authPers1.type = 1;
	
	authPers2.name = "Sotia";
	authPers2.code = "RFID9990002";
	authPers2.type = 1;
	
	dog1.name = "Rex";
	dog1.code = "RFID99900015";
	dog1.type = 2;
	
	cat1.name = "Lola";
	cat1.code = "RFID99900025";
	cat1.type = 2;
	
	authPersonal[0] = authPers1;
	authPersonal[1] = authPers2;
	authPersonal[2] = dog1;
	authPersonal[3] = cat1;
}

/* Parametrii:
 * @enforcementAlarm - alarma care se declanseaza in cazul incercarii intrarii fortate de catre o persoana neautorizata
 * @lightOnIn        - pentru a aprinde lumina din interior in caz de detectare a miscarii
 * @lightOnOut       - pentru a aprinde lumina din exterior in caz de detectare a miscarii
 * @mainDoorLock     - ne indica daca usa principala (pentru oameni) este deschisa/inchisa
 * @petDoorLock      - ne indica daca usa secundara (pentru animale) este deschisa/inchisa 
*/  
string rfid, pin_code;
int movementValueInside, movementValueOutside, forceLevel;
bool enforcementAlarm = false, lightOnIn = false, lightOnOut = false, mainDoorLock = false, petDoorLock = false; 


void iesire(string outData, string name){
    ofstream out;
    out.open(name, std::ios_base::app);
    out << outData;
}

bool is_digits(const string &str) {
    return str.find_first_not_of("0123456789") == string::npos;
}

void readyTest(const Rest::Request& request, Http::ResponseWriter response) {
    response.send(Http::Code::Ok, "It's all good!\n");
} 

// Sensor Settings
void setSensorValue(const Rest::Request& request, Http::ResponseWriter response) {
    auto sensor_type = request.param(":type").as<std::string>();     // "RFID" or "movement" or "PIN" or "shock"
	auto location    = request.param(":location").as<std::string>(); // Pentru RFID si PIN doar "outside", pentru movement si shock avem "inside" si "outside" 
    auto value       = request.param(":value").as<std::string>();
 
	if(sensor_type.compare("RFID") != 0 && sensor_type.compare("PIN") != 0 && sensor_type.compare("movement") != 0 && sensor_type.compare("shock") != 0) {
        response.send(Http::Code::Not_Found, "Option poate avea valorile RFID sau movement sau PIN sau shock\n");
        return;
    }
	
    if(location.compare(INSIDE) != 0 && location.compare(OUTSIDE) != 0) {
        response.send(Http::Code::Not_Found, "Location poate avea doar valorile outside sau inside.\n");
        return;
    }
	
	if(sensor_type.compare("RFID") == 0) {
        if(location.compare(OUTSIDE) == 0) {
			rfid = value;
			string info, info2;
			
			for(int i = 0; i < 4; i++){
				if(rfid.compare(authPersonal[i].code) == 0){
					if(authPersonal[i].type == 1) {
						info2 = " usa principala pentru un om\n";
						mainDoorLock = true;					
					} else {
						info2 = " usita pentru animalul de companie\n";
						petDoorLock = true;
					}
				
					info = authPersonal[i].name + " intra in casa, se deschide " + info2;                    

					iesire(info, "auth_personal.txt");
				}
			}
        }   
    }
    
    if(sensor_type.compare("PIN") == 0) {
        if(location.compare(OUTSIDE) == 0) {
            pin_code = value;
			
			if(pin_code.compare(PIN) == 0){
				string info = "A intrat cineva utilizand codul PIN\n";
				
				mainDoorLock = true;
				
				iesire(info, "auth_personal.txt");
			}
        }
    }
	
	if(sensor_type.compare("movement") == 0) {
        if(location.compare(INSIDE) == 0) {
            if(!is_digits(value)) {
				response.send(Http::Code::Not_Found, "Value poate fi doar un numar intreg.\n");
				return;
			}
			
			movementValueInside = stoi(value);
			
			if(movementValueInside > 7) {
				lightOnIn = true;
				
				string info = "S-a detectat miscare din interior cu valoarea " + value + "\n";
				iesire(info, "movement_and_shock.txt");
			}
        }
		
		if(location.compare(OUTSIDE) == 0) {
			if(!is_digits(value)) {
				response.send(Http::Code::Not_Found, "Value poate fi doar un numar intreg.\n");
				return;
			}
			
			movementValueOutside = stoi(value);
			
			if(movementValueInside > 7) {
				lightOnOut = true;
				
				string info = "S-a detectat miscare din exterior cu valoarea " + value + "\n";
				iesire(info, "movement_and_shock.txt");
			}
        }
    }
	
	if(sensor_type.compare("shock") == 0) {
        if(location.compare(INSIDE) == 0 || location.compare(OUTSIDE) == 0) {
			if(!is_digits(value)) {
				response.send(Http::Code::Not_Found, "Value poate fi doar un numar intreg.\n");
				return;
			}
			
            forceLevel = stoi(value);
			
			if(forceLevel > 9000) {
				enforcementAlarm = true;
				
				string info = "S-a detectat un soc puternic cu intensitatea " + value + "\n";
				iesire(info, "movement_and_shock.txt");
			}
        }
    }

    response.send(Http::Code::Ok, "Sensors value set\n");
}

void getSensorValue(const Rest::Request& request, Http::ResponseWriter response) {
	auto sensor_type = request.param(":type").as<std::string>();   
	auto location    = request.param(":location").as<std::string>(); 
 
	string info;
 
    if(sensor_type.compare("RFID") != 0 && sensor_type.compare("PIN") != 0 && sensor_type.compare("movement") != 0 && sensor_type.compare("shock") != 0) {
        response.send(Http::Code::Not_Found, "Option poate avea valorile RFID sau movement sau PIN sau shock\n");
        return;
    }
	
    if(location.compare(INSIDE) != 0 && location.compare(OUTSIDE) != 0) {
        response.send(Http::Code::Not_Found, "Location poate avea doar valorile outside sau inside.\n");
        return;
    }

    if(sensor_type.compare("RFID") == 0) {
        if(location.compare(OUTSIDE) == 0) {
			info = "Tipul de senzor RFID detectat din exterior cu valoarea ";
            info += rfid;
        } else {
			info = "Nu se iese pe baza de RFID"; //idiot proof system
		}  
    }
    
    if(sensor_type.compare("PIN") == 0) {
        if(location.compare(OUTSIDE) == 0) {		
			if(pin_code.compare(PIN) == 0){
				info = "A intrat cineva utilizand codul PIN";
			} else {
				info = "Pin gresit";
			}
        } else {
			info = "Nu se iese pe baza de pin";
		}
    }
	
	if(sensor_type.compare("movement") == 0) {
        if(location.compare(INSIDE) == 0) {
			info = "S-a detectat inauntru miscare cu valoarea ";
			
            info += to_string(movementValueInside);
        }
		
		if(location.compare(OUTSIDE) == 0) {
			info = "S-a detectat afara miscare cu valoarea ";
			
            info += to_string(movementValueOutside);
        }
    }
	
	if(sensor_type.compare("shock") == 0) {
        if(location.compare(INSIDE) == 0 || location.compare(OUTSIDE) == 0) {
			info = "S-a detectat un soc cu intensitatea ";
			
            info += to_string(forceLevel);
        }
    }

    response.send(Http::Code::Ok, info + "\n");
}


/*void mqtt() {
    const std::string address = "localhost";
    const std::string clientId = "Portal";

    // Create a client
    mqtt::client client(address, clientId);

    mqtt::connect_options options;
    options.set_keep_alive_interval(20);
    options.set_clean_session(true);

    try {
        // Connect to the client
        client.connect(options);

        // Create a message
        const std::string TOPIC = "Portal";
        const std::string PAYLOAD = "Yes, I'm back";
        auto msg = mqtt::make_message(TOPIC, PAYLOAD);

        // Publish it to the server
        client.publish(msg);

        // Disconnect
        client.disconnect();
    }
    catch (const mqtt::exception& exc) {
        std::cerr << exc.what() << " [" << exc.get_reason_code() << "]" << std::endl;
    }
}*/


void setupRoutes() {
    // Ruta de test pentru libraria Pistache
    router.get("/ready", Routes::bind(readyTest));

    // Rute pentru setarea si returnarea valorilor senzorilor
    router.get("/sensor/set/:type/:location/:value", Routes::bind(setSensorValue));
    router.get("/sensor/get/:type/:location", Routes::bind(getSensorValue));
}

void start() {
    auto opts = Http::Endpoint::options().threads(1);
	
    portal.init(opts);
    portal.setHandler(router.handler());
    portal.serveThreaded(); 
}

// When signaled server shuts down
void stop(){
    portal.shutdown();
}

int main() {
    //mqtt();
	initAuthPersonal();
	// This code is needed for gracefull shutdown of the server when no longer needed.
    sigset_t signals;
    if (sigemptyset(&signals) != 0
            || sigaddset(&signals, SIGTERM) != 0
            || sigaddset(&signals, SIGINT) != 0
            || sigaddset(&signals, SIGHUP) != 0
            || pthread_sigmask(SIG_BLOCK, &signals, nullptr) != 0) {
        perror("install signal handler failed");
        return 1;
    }
    
	setupRoutes();
	
	cout << "We rock" << endl;
	
	start();

    // Code that waits for the shutdown sinal for the server
    int signal = 0;
    int status = sigwait(&signals, &signal);
    if (status == 0)
    {
        std::cout << "received signal " << signal << std::endl;
    }
    else
    {
        std::cerr << "sigwait returns " << status << std::endl;
    }

    stop(); 
}