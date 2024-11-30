#include <ESP8266WiFi.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

// Pins for IR sensors and button
#define START_IR_PIN 5
#define END_IR_PIN 4
#define BUTTON_PIN 16

// WiFi credentials for Access Point
const char* ap_ssid = "MicromouseCompetition";
const char* password = "12345678";

WiFiServer server(80);

// Competition Configuration
const int MAX_TEAMS = 6;
const int MAX_ATTEMPTS = 4;
const unsigned long MAX_TIME_LIMIT = 600000; // 10 minutes in milliseconds

// Data Structure for Team and Attempts
struct Attempt {
    float timeTaken = 0.0;
    bool completed = false;
};

struct Team {
    int id;
    String name;
    float bestTime = 0.0;
    Attempt attempts[MAX_ATTEMPTS];
    int currentAttempt = 0;
};

// Global Variables
Team teams[MAX_TEAMS];
int currentTeamIndex = -1;
unsigned long startTime = 0;
unsigned long remainingTime = MAX_TIME_LIMIT;
bool isRunning = false;

void setup() {
    Serial.begin(115200);

    // Initialize Pins
    pinMode(START_IR_PIN, INPUT);
    pinMode(END_IR_PIN, INPUT);
    pinMode(BUTTON_PIN, INPUT_PULLUP);

    // Initialize Teams
    initializeTeams();

    // Setup WiFi Access Point
    WiFi.softAP(ap_ssid, password);
    IPAddress ip = WiFi.softAPIP();
    Serial.println("Access Point IP: " + ip.toString());

    // Initialize LittleFS
    if (!LittleFS.begin()) {
        Serial.println("LittleFS mount failed");
        return;
    }

    // Load existing data or create default
    loadTeamsData();

    // Start Web Server
    server.begin();
}

void loop() {
    handleWebClients();
    handleTimerLogic();
}

void handleWebClients() {
    WiFiClient client = server.available();
    if (!client) return;

    String request = client.readStringUntil('\r');
    String method = request.substring(0, request.indexOf(' '));
    String path = request.substring(request.indexOf(' ') + 1);
    path = path.substring(0, path.indexOf(' '));
    
    Serial.println("Received request: " + method + " " + path);

    // Wait for headers to complete
    while (client.available()) {
        String line = client.readStringUntil('\r');
        if (line == "\n") break;
    }

    if (method == "POST" && path.startsWith("/select-team")) {
        handleTeamSelection(client, request);
    } else if (method == "POST" && path.startsWith("/reset-team")) {
        handleTeamReset(client, request);
    } else if (path.startsWith("/team-data")) {
        sendTeamData(client);
    } else if (path.startsWith("/teams-data")) {
        sendAllTeamsData(client);
    } else if (path.startsWith("/")) {
        serveFile(client, path);
    }
}

void handleTeamSelection(WiFiClient& client, const String& request) {
    // Extract team ID from the request body
    String body = "";
    while (client.available()) {
        body += (char)client.read();
    }
    
    int startPos = body.indexOf("id=");
    int teamId = -1;
    
    if (startPos != -1) {
        teamId = body.substring(startPos + 3).toInt();
        Serial.println("Received team selection request for ID: " + String(teamId));
    }
    
    // Validate team ID
    if (teamId > 0 && teamId <= MAX_TEAMS) {
        currentTeamIndex = teamId - 1;
        
        // Don't reset attempts when selecting team, only when explicitly requested
        // resetTeamAttempts(currentTeamIndex);
        
        // Save the updated data
        saveTeamsData();
        
        // Send success response
        sendJsonResponse(client, "{\"status\":\"success\",\"teamId\":" + String(teamId) + "}");
    } else {
        // Send error response
        sendJsonResponse(client, "{\"status\":\"error\",\"message\":\"Invalid team ID\"}");
    }
}

void handleTeamReset(WiFiClient& client, const String& request) {
    // Extract team ID from the request body
    String body = "";
    while (client.available()) {
        body += (char)client.read();
    }
    
    int startPos = body.indexOf("id=");
    int teamId = -1;
    
    if (startPos != -1) {
        teamId = body.substring(startPos + 3).toInt();
    }
    
    if (teamId > 0 && teamId <= MAX_TEAMS) {
        resetTeamAttempts(teamId - 1);
        saveTeamsData();  // Important: Save the data after reset
        sendJsonResponse(client, "{\"status\":\"success\",\"teamId\":" + String(teamId) + "}");
    } else {
        sendJsonResponse(client, "{\"status\":\"error\",\"message\":\"Invalid team ID\"}");
    }
}

void handleTimerLogic() {
    if (currentTeamIndex == -1) return;

    // Start timing when START IR triggered
    if (digitalRead(START_IR_PIN) == LOW && !isRunning) {
        startTime = millis();
        isRunning = true;
    }

    // Stop timing when END IR triggered
    if (digitalRead(END_IR_PIN) == LOW && isRunning) {
        unsigned long endTime = millis();
        float timeTaken = (endTime - startTime) / 1000.0;

        // Update current attempt
        Team& currentTeam = teams[currentTeamIndex];
        Attempt& currentAttempt = currentTeam.attempts[currentTeam.currentAttempt];
        
        currentAttempt.timeTaken = timeTaken;
        currentAttempt.completed = true;

        // Update best time
        if (currentTeam.bestTime == 0 || timeTaken < currentTeam.bestTime) {
            currentTeam.bestTime = timeTaken;
        }

        // Move to next attempt
        currentTeam.currentAttempt++;

        // Reset for next run
        isRunning = false;

        // Save updated data
        saveTeamsData();
    }
}

void initializeTeams() {
    for (int i = 0; i < MAX_TEAMS; i++) {
        teams[i].id = i + 1;
        teams[i].name = "Đội " + String(i + 1);
    }
}

void loadTeamsData() {
    File dataFile = LittleFS.open("/teams.json", "r");
    if (!dataFile) {
        Serial.println("No saved data, using defaults");
        saveTeamsData();
        return;
    }

    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, dataFile);
    
    if (error) {
        Serial.println("Failed to parse JSON");
        dataFile.close();
        return;
    }

    // Populate teams data from JSON
    JsonArray teamsArray = doc["teams"];
    for (int i = 0; i < MAX_TEAMS && i < teamsArray.size(); i++) {
        JsonObject teamObj = teamsArray[i];
        
        teams[i].id = teamObj["id"] | (i + 1);
        teams[i].name = teamObj["name"] | ("Đội " + String(i + 1));
        teams[i].bestTime = teamObj["bestTime"] | 0.0;
        teams[i].currentAttempt = teamObj["currentAttempt"] | 0;
        
        JsonArray attemptsArray = teamObj["attempts"];
        for (int j = 0; j < MAX_ATTEMPTS && j < attemptsArray.size(); j++) {
            JsonObject attemptObj = attemptsArray[j];
            teams[i].attempts[j].timeTaken = attemptObj["timeTaken"] | 0.0;
            teams[i].attempts[j].completed = attemptObj["completed"] | false;
        }
    }

    dataFile.close();
}

void saveTeamsData() {
    DynamicJsonDocument doc(2048);
    JsonArray teamsArray = doc.createNestedArray("teams");

    for (int i = 0; i < MAX_TEAMS; i++) {
        JsonObject teamObj = teamsArray.createNestedObject();
        teamObj["id"] = teams[i].id;
        teamObj["name"] = teams[i].name;
        teamObj["bestTime"] = teams[i].bestTime;

        JsonArray attemptsArray = teamObj.createNestedArray("attempts");
        for (int j = 0; j < MAX_ATTEMPTS; j++) {
            JsonObject attemptObj = attemptsArray.createNestedObject();
            attemptObj["timeTaken"] = teams[i].attempts[j].timeTaken;
            attemptObj["completed"] = teams[i].attempts[j].completed;
        }
    }

    File dataFile = LittleFS.open("/teams.json", "w");
    if (!dataFile) {
        Serial.println("Failed to open file for writing");
        return;
    }

    serializeJson(doc, dataFile);
    dataFile.close();
}

void resetTeamAttempts(int teamIndex) {
    if (teamIndex < 0 || teamIndex >= MAX_TEAMS) return;
    
    teams[teamIndex].currentAttempt = 0;
    teams[teamIndex].bestTime = 0.0;
    
    for (int i = 0; i < MAX_ATTEMPTS; i++) {
        teams[teamIndex].attempts[i].timeTaken = 0.0;
        teams[teamIndex].attempts[i].completed = false;
    }
    
    // Make sure to save after resetting
    saveTeamsData();
}

void sendTeamData(WiFiClient& client) {
    if (currentTeamIndex == -1) {
        client.println("HTTP/1.1 400 Bad Request");
        return;
    }

    DynamicJsonDocument doc(1024);
    Team& team = teams[currentTeamIndex];
    
    doc["id"] = team.id;
    doc["name"] = team.name;
    doc["bestTime"] = team.bestTime;
    
    JsonArray attemptsArray = doc.createNestedArray("attempts");
    for (int i = 0; i < MAX_ATTEMPTS; i++) {
        JsonObject attemptObj = attemptsArray.createNestedObject();
        attemptObj["timeTaken"] = team.attempts[i].timeTaken;
        attemptObj["completed"] = team.attempts[i].completed;
    }

    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: application/json");
    client.println("Connection: close");
    client.println();

    String jsonResponse;
    serializeJson(doc, jsonResponse);
    client.println(jsonResponse);
}

void sendAllTeamsData(WiFiClient& client) {
    DynamicJsonDocument doc(2048);
    JsonArray teamsArray = doc.createNestedArray("teams");

    for (int i = 0; i < MAX_TEAMS; i++) {
        JsonObject teamObj = teamsArray.createNestedObject();
        teamObj["id"] = teams[i].id;
        teamObj["name"] = teams[i].name;
        teamObj["bestTime"] = teams[i].bestTime;
        
        // Add attempts array
        JsonArray attemptsArray = teamObj.createNestedArray("attempts");
        for (int j = 0; j < MAX_ATTEMPTS; j++) {
            JsonObject attemptObj = attemptsArray.createNestedObject();
            attemptObj["timeTaken"] = teams[i].attempts[j].timeTaken;
            attemptObj["completed"] = teams[i].attempts[j].completed;
        }
    }

    // Send proper HTTP headers first
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: application/json");
    client.println("Connection: close");
    client.println();

    // Send the JSON response
    serializeJson(doc, client);
}

// Helper functions for web server
int extractTeamId(const String& request) {
    int startIndex = request.indexOf("id=") + 3;
    int endIndex = request.indexOf(" ", startIndex);
    return request.substring(startIndex, endIndex).toInt();
}

void sendJsonResponse(WiFiClient& client, const String& jsonResponse) {
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: application/json");
    client.println("Connection: close");
    client.println();
    client.println(jsonResponse);
}

void serveFile(WiFiClient& client, const String& request) {
    String path = request.substring(request.indexOf(" ") + 1, request.lastIndexOf(" "));
    if (path == "/") path = "/index.html";

    File file = LittleFS.open(path, "r");
    if (!file) {
        client.println("HTTP/1.1 404 Not Found");
        client.println("Content-Type: text/plain");
        client.println();
        client.println("404 File Not Found");
        return;
    }

    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: " + getContentType(path));
    client.println("Connection: close");
    client.println();

    while (file.available()) {
        client.write(file.read());
    }
    file.close();
}

String getContentType(const String& path) {
    if (path.endsWith(".html")) return "text/html";
    if (path.endsWith(".css")) return "text/css";
    if (path.endsWith(".js")) return "application/javascript";
    if (path.endsWith(".png")) return "image/png";
    return "text/plain";
}