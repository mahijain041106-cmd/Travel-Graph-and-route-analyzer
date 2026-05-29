#include <iostream>
#include <vector>
#include <queue>
#include <math.h>
#include <unordered_map>
#include <string>
#include <fstream>
#include <algorithm>
using namespace std;

pair<double, double> getCoordinates(string place)
{

    string command = "curl -s \"https://nominatim.openstreetmap.org/search?format=json&q=" + place + "\" > temp.json";
    system(command.c_str());

    ifstream file("temp.json");
    string data((istreambuf_iterator<char>(file)), istreambuf_iterator<char>());
    file.close();

    size_t latPos = data.find("\"lat\":\"");
    size_t lonPos = data.find("\"lon\":\"");

    double lat = stod(data.substr(latPos + 7, 10));
    double lon = stod(data.substr(lonPos + 7, 10));

    return {lat, lon};
}

int main()
{
    string src, dest;

    cout << "Enter Source: ";
    getline(cin, src);

    cout << "Enter Destination: ";
    getline(cin, dest);

    auto s = getCoordinates(src);
    auto d = getCoordinates(dest);

    ofstream out("coords.json");
    out << "{\n";
    out << "\"src\": [" << s.first << "," << s.second << "],\n";
    out << "\"dest\": [" << d.first << "," << d.second << "]\n";
    out << "}";
    out.close();

    cout << "coords.json created!\n";
}