#include<iostream>
#include<vector>
#include<queue>
#include<math.h>
#include<unordered_map>
#include<string>
#include<fstream>
#include<algorithm>
#include<sstream>
#include<cstdlib>
using namespace std;

//FOR COORDINATES
pair<double,double> getCoordinates(string place) {
    string command = "curl -s \"[nominatim.openstreetmap.org](https://nominatim.openstreetmap.org/search?format=json&q=)" + place + "\" > temp.json";
    system(command.c_str());

    ifstream file("temp.json");
    string data((istreambuf_iterator<char>(file)), istreambuf_iterator<char>());
    file.close();

    size_t latPos = data.find("\"lat\":\"");
    size_t lonPos = data.find("\"lon\":\"");

    if(latPos == string::npos || lonPos == string::npos) {
        return {0.0, 0.0};
    }

    double lat = stod(data.substr(latPos + 7, 10));
    double lon = stod(data.substr(lonPos + 7, 10));

    return {lat, lon};
}

//TO CONVERT STRING LOCATION NAME TO LOWERCASE
string toLowerString(const string& input) {
    string result = input;
    transform(result.begin(), result.end(), result.begin(),
              [](unsigned char c){ return std::tolower(c); });
    return result;
}

//TO CHECK IF THE LOCATION ALREADY EXISTS IN THE FILE OR NOT
bool isDuplicate(string name, double lat, double lon) {
    ifstream file("coords.txt");

    string existingName;
    double existingLat, existingLon;

    name = toLowerString(name);

    while(file >> existingName >> existingLat >> existingLon) {
        existingName = toLowerString(existingName);
        if(existingName == name)
            return true;
        if(abs(existingLat - lat) < 0.001 &&
           abs(existingLon - lon) < 0.001)
            return true;
    }
    file.close();
    return false;
}

//TO ADD A NEW LOCATION
void addLocation(string source,string dest) {
    ofstream file("coords.txt", ios::app);

    auto coord1 = getCoordinates(source);
    if(coord1.first != 0.0 || coord1.second != 0.0) {
        if(!isDuplicate(source, coord1.first, coord1.second)) {
            file << source << " " << coord1.first << " " << coord1.second << endl;
        }
    }

    auto coord2 = getCoordinates(dest);
    if(coord2.first != 0.0 || coord2.second != 0.0) {
        if(!isDuplicate(dest, coord2.first, coord2.second)) {
            file << dest << " " << coord2.first << " " << coord2.second << endl;
        }
    }
    file.close();
}

//STRUCTURE OF A NODE IN A GRAPH
struct Node {
    string name;
    double lat, lon;
};

vector<Node> nodes;
unordered_map<string, int> cityIndex;
vector<vector<pair<int,double>>> adj;

//DISTANCE CALCULATE
double getDistance(double lat1, double lon1, double lat2, double lon2) {
    return sqrt(pow(lat1-lat2,2) + pow(lon1-lon2,2));
}

//READ FILE
void loadData() {
    nodes.clear();
    cityIndex.clear();
    adj.clear();

    ifstream file("coords.txt");
    string name;
    double lat, lon;

    while(file >> name >> lat >> lon) {
        nodes.push_back({name, lat, lon});
        cityIndex[name] = nodes.size() - 1;
        adj.push_back({});
    }
    file.close();
}

//BUILD GRAPH
void buildGraph() {
    int n = nodes.size();
    adj.assign(n, {});

    for(int i = 0; i < n; i++) {
        vector<pair<double,int>> distList;

        for(int j = 0; j < n; j++) {
            if(i == j) continue;

            double dist = getDistance(
                nodes[i].lat, nodes[i].lon,
                nodes[j].lat, nodes[j].lon
            );

            distList.push_back({dist, j});
        }

        sort(distList.begin(), distList.end());

        for(int k = 0; k < 6 && k < (int)distList.size(); k++) {
            int neighbor = distList[k].second;
            double dist = distList[k].first;

            adj[i].push_back({neighbor, dist});
            adj[neighbor].push_back({i, dist});
        }
    }
}

//PATH FIND FOR DFS
bool pathfind(int node, int dest, vector<int>& visited, vector<int>& path)
{
    visited[node] = 1;
    path.push_back(node);

    if(node == dest)
        return true;

    for(auto neigh : adj[node])
    {
        int next = neigh.first;

        if(!visited[next])
        {
            if(pathfind(next, dest, visited, path))
                return true;
        }
    }

    path.pop_back();
    return false;
}

//DFS IMPLEMENTATION
void dfs(string source, string dest)
{
    if(cityIndex.find(source) == cityIndex.end() ||
       cityIndex.find(dest) == cityIndex.end()) {
        cout << "Invalid locations\n";
        return;
    }

    int start = cityIndex[source];
    int end = cityIndex[dest];

    vector<int> visited(nodes.size(), 0);
    vector<int> path;

    if(pathfind(start, end, visited, path))
    {
        cout << "\nDFS Path: ";
        for(auto i : path)
            cout << nodes[i].name << " -> ";
        cout << "END\n";
    }
    else
    {
        cout << "No path found (DFS)\n";
    }
}

//BFS IMPLEMENTATION
void bfs(string source, string dest)
{
    if(cityIndex.find(source) == cityIndex.end() ||
       cityIndex.find(dest) == cityIndex.end()) {
        cout << "Invalid locations\n";
        return;
    }

    int start = cityIndex[source];
    int end = cityIndex[dest];

    vector<int> visited(nodes.size(), 0);
    vector<int> parent(nodes.size(), -1);

    queue<int> q;
    q.push(start);
    visited[start] = 1;

    while(!q.empty())
    {
        int node = q.front();
        q.pop();

        if(node == end) break;

        for(auto neigh : adj[node])
        {
            int next = neigh.first;

            if(!visited[next])
            {
                visited[next] = 1;
                parent[next] = node;
                q.push(next);
            }
        }
    }

    if(!visited[end])
    {
        cout << "No path found (BFS)\n";
        return;
    }

    vector<int> path;
    for(int v = end; v != -1; v = parent[v])
        path.push_back(v);

    reverse(path.begin(), path.end());

    cout << "\nBFS Path: ";
    for(auto i : path)
        cout << nodes[i].name << " -> ";
    cout << "END\n";
}

//DIJKSTRA IMPLEMENTATION
void dijkstra(string source, string dest)
{
    if(cityIndex.find(source) == cityIndex.end() ||
       cityIndex.find(dest) == cityIndex.end()) {
        cout << "Invalid locations\n";
        return;
    }

    int start = cityIndex[source];
    int end = cityIndex[dest];
    int n = nodes.size();

    vector<double> distance(n, 1e9);
    vector<int> parent(n, -1);

    priority_queue<pair<double,int>,
                   vector<pair<double,int>>,
                   greater<pair<double,int>>> pq;

    distance[start] = 0;
    pq.push({0, start});

    while(!pq.empty())
    {
        double dis = pq.top().first;
        int node = pq.top().second;
        pq.pop();

        if(dis > distance[node]) continue;

        for(auto adjnode : adj[node])
        {
            int adjvertex = adjnode.first;
            double adjwgt = adjnode.second;

            double d = dis + adjwgt;

            if(d < distance[adjvertex])
            {
                parent[adjvertex] = node;
                distance[adjvertex] = d;
                pq.push({d, adjvertex});
            }
        }
    }

    if(distance[end] == 1e9)
    {
        cout << "No path found (Dijkstra)\n";
        return;
    }

    vector<int> path;
    for(int v = end; v != -1; v = parent[v])
        path.push_back(v);

    reverse(path.begin(), path.end());

    cout << "\nShortest Path: ";
    for(auto i : path)
        cout << nodes[i].name << " -> ";
    cout << "END\n";
}

// ---------- USER ----------
struct user
{
    string email;
    string password;
};

// ---------- TRIP ----------
struct trip
{
    string email;
    string source;
    string destination;
    string date;
    int distance;
    int cost;
};

// ---------- SIGN UP ----------
void signup()
{
    user u;
    ofstream file("users.txt", ios::app);

    cout << "\n--- SIGN UP ---\n";
    cout << "Enter Email: ";
    cin >> u.email;

    cout << "Create Password: ";
    cin >> u.password;

    file << u.email << "|" << u.password << endl;
    file.close();

    cout << "Account Created!\n";
}

// ---------- LOGIN ----------
int login(string &currentUser)
{
    ifstream file("users.txt");
    user u;
    string email, pass;

    cout << "\n--- LOGIN ---\n";
    cout << "Email: ";
    cin >> email;

    cout << "Password: ";
    cin >> pass;

    while(getline(file, u.email, '|') &&
          getline(file, u.password))
    {
        if(email == u.email && pass == u.password)
        {
            currentUser = email;
            file.close();
            return 1;
        }
    }

    file.close();
    return 0;
}

// ---------- ADD TRIP ----------
void add_trip(string currentUser)
{
    trip t;
    t.email = currentUser;

    cout << "\nEnter Source: ";
    cin >> t.source;

    cout << "Enter Destination: ";
    cin >> t.destination;

    cout << "Enter Date: ";
    cin >> t.date;

    cout << "Enter Distance: ";
    cin >> t.distance;

    cout << "Enter Cost: ";
    cin >> t.cost;

    ofstream file("trips.txt", ios::app);

    file << t.email << "|" << t.source << "|" << t.destination << "|"
         << t.date << "|" << t.distance << "|" << t.cost << endl;

    file.close();

    cout << "Trip Saved!\n";
}

// ---------- VIEW TRIPS ----------
void view_trips(string currentUser)
{
    ifstream file("trips.txt");
    trip t;

    cout << "\n---- Your Trips ----\n";

    while(getline(file, t.email, '|') &&
          getline(file, t.source, '|') &&
          getline(file, t.destination, '|') &&
          getline(file, t.date, '|') &&
          file >> t.distance &&
          file.ignore() &&
          file >> t.cost &&
          file.ignore())
    {
        if(t.email == currentUser)
        {
            cout << "------------------\n";
            cout << "From: " << t.source << endl;
            cout << "To: " << t.destination << endl;
            cout << "Date: " << t.date << endl;
            cout << "Distance: " << t.distance << endl;
            cout << "Cost: " << t.cost << endl;
        }
    }

    file.close();
}

// ---------- RULE-BASED CHATBOT ----------
void chatbotHelp()
{
    cout << "\n========== SMARTROUTEX CHATBOT ==========\n";
    cout << "You can type commands like:\n";
    cout << "1. signup\n";
    cout << "2. login\n";
    cout << "3. add trip\n";
    cout << "4. view trips\n";
    cout << "5. find path\n";
    cout << "6. dfs\n";
    cout << "7. bfs\n";
    cout << "8. dijkstra\n";
    cout << "9. logout\n";
    cout << "10. exit\n";
    cout << "11. help\n";
    cout << "=========================================\n";
}

void explainAlgorithm(const string &msg)
{
    if(msg == "dfs")
    {
        cout << "\nChatbot: DFS stands for Depth First Search.\n";
        cout << "It explores one path deeply before backtracking.\n";
        cout << "Use it when you want to traverse possible routes.\n";
    }
    else if(msg == "bfs")
    {
        cout << "\nChatbot: BFS stands for Breadth First Search.\n";
        cout << "It explores level by level and finds the path with minimum edges.\n";
    }
    else if(msg == "dijkstra")
    {
        cout << "\nChatbot: Dijkstra finds the shortest weighted path.\n";
        cout << "Use it when distance between cities matters.\n";
    }
}

void chatbot(string &currentUser, bool &loggedIn)
{
    cin.ignore();
    string input;

    chatbotHelp();

    while(true)
    {
        cout << "\nYou: ";
        getline(cin, input);
        input = toLowerString(input);

        if(input == "help")
        {
            chatbotHelp();
        }
        else if(input == "signup")
        {
            cout << "Chatbot: Redirecting to Sign Up...\n";
            signup();
        }
        else if(input == "login")
        {
            cout << "Chatbot: Redirecting to Login...\n";
            if(login(currentUser))
            {
                loggedIn = true;
                cout << "Chatbot: Login successful.\n";
                return;
            }
            else
            {
                cout << "Chatbot: Invalid login credentials.\n";
            }
        }
        else if(input == "add trip")
        {
            if(loggedIn)
            {
                cout << "Chatbot: Opening Add Trip...\n";
                add_trip(currentUser);
            }
            else
            {
                cout << "Chatbot: Please login first.\n";
            }
        }
        else if(input == "view trips")
        {
            if(loggedIn)
            {
                cout << "Chatbot: Showing your saved trips...\n";
                view_trips(currentUser);
            }
            else
            {
                cout << "Chatbot: Please login first.\n";
            }
        }
        else if(input == "find path")
        {
            if(loggedIn)
            {
                string source, dest;
                int c;

                cout << "Chatbot: Enter source: ";
                cin >> source;
                cout << "Chatbot: Enter destination: ";
                cin >> dest;

                source = toLowerString(source);
                dest = toLowerString(dest);

                addLocation(source, dest);
                loadData();
                buildGraph();

                cout << "\nChoose algorithm:\n";
                cout << "1. DFS\n2. BFS\n3. Dijkstra\n";
                cin >> c;

                if(c == 1) dfs(source, dest);
                else if(c == 2) bfs(source, dest);
                else if(c == 3) dijkstra(source, dest);
                else cout << "Chatbot: Invalid choice.\n";

                cin.ignore();
            }
            else
            {
                cout << "Chatbot: Please login first.\n";
            }
        }
        else if(input == "dfs" || input == "bfs" || input == "dijkstra")
        {
            explainAlgorithm(input);
        }
        else if(input == "logout")
        {
            if(loggedIn)
            {
                loggedIn = false;
                currentUser = "";
                cout << "Chatbot: You have been logged out.\n";
            }
            else
            {
                cout << "Chatbot: No user is currently logged in.\n";
            }
        }
        else if(input == "exit")
        {
            cout << "Chatbot: Exiting chatbot.\n";
            return;
        }
        else
        {
            cout << "Chatbot: Sorry, I didn't understand that.\n";
            cout << "Type 'help' to see available commands.\n";
        }
    }
}

// ---------- TRIP MANAGER ----------
void trip_manager(string currentUser)
{
    int ch;
    string source, dest;
    int c;
    bool loggedIn = true;

    while(1)
    {
        cout << "\n--- Trip Manager ---\n";
        cout << "1.Add Trip\n";
        cout << "2.View Trips\n";
        cout << "3.Find Path\n";
        cout << "4.Chatbot\n";
        cout << "5.Logout\n";

        cin >> ch;

        switch(ch)
        {
            case 1:
                add_trip(currentUser);
                break;

            case 2:
                view_trips(currentUser);
                break;

            case 3:
                cout << "\nEnter source: ";
                cin >> source;
                source = toLowerString(source);

                cout << "\nEnter destination: ";
                cin >> dest;
                dest = toLowerString(dest);

                addLocation(source, dest);
                loadData();
                buildGraph();

                cout << "\nEnter your choice:\n1.DFS\n2.BFS\n3.Dijkstra\n";
                cin >> c;

                switch(c)
                {
                    case 1: dfs(source, dest); break;
                    case 2: bfs(source, dest); break;
                    case 3: dijkstra(source, dest); break;
                    default: cout << "Invalid choice\n";
                }
                break;

            case 4:
                chatbot(currentUser, loggedIn);
                if(!loggedIn) return;
                break;

            case 5:
                return;

            default:
                cout << "Invalid choice\n";
        }
    }
}

// ---------- HOME PAGE ----------
void homepage()
{
    int ch;
    string currentUser;
    bool loggedIn = false;

    while(1)
    {
        cout << "\n=====================\n";
        cout << "    SMARTROUTEX\n";
        cout << "=====================\n";

        cout << "1.Login\n";
        cout << "2.Sign Up\n";
        cout << "3.Chatbot\n";
        cout << "4.Exit\n";

        cout << "\nIf not registered? Sign Up\n";

        cin >> ch;

        switch(ch)
        {
            case 1:
                if(login(currentUser))
                {
                    cout << "Login Successful!\n";
                    trip_manager(currentUser);
                }
                else
                {
                    cout << "Invalid Login!\n";
                }
                break;

            case 2:
                signup();
                break;

            case 3:
                chatbot(currentUser, loggedIn);
                if(loggedIn)
                    trip_manager(currentUser);
                break;

            case 4:
                return;

            default:
                cout << "Invalid choice\n";
        }
    }
}

int main()
{
    homepage();
    return 0;
}