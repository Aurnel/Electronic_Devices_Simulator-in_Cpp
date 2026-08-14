#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>
#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <set>
#include <unordered_map>
#include <iterator>
#include <fstream>
#include <deque>
#include <utility>

#ifdef _WIN32
#include <windows.h>

std::string openFileDialog() {
    HMODULE hComdlg = LoadLibraryA("comdlg32.dll");
    if (!hComdlg) return "";
    typedef BOOL(WINAPI *GetOpenFileNameA_Func)(LPOPENFILENAMEA);
    GetOpenFileNameA_Func getOpenFileNameA = (GetOpenFileNameA_Func)GetProcAddress(hComdlg, "GetOpenFileNameA");
    
    if (getOpenFileNameA) {
        char filename[MAX_PATH] = {0};
        OPENFILENAMEA ofn;
        ZeroMemory(&ofn, sizeof(ofn));
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = NULL;
        ofn.lpstrFilter = "Text Files\0*.txt\0All Files\0*.*\0";
        ofn.lpstrFile = filename;
        ofn.nMaxFile = MAX_PATH;
        ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
        ofn.lpstrDefExt = "txt";
        if (getOpenFileNameA(&ofn)) {
            FreeLibrary(hComdlg);
            return std::string(filename);
        }
    }
    FreeLibrary(hComdlg);
    return "";
}

std::string saveFileDialog() {
    HMODULE hComdlg = LoadLibraryA("comdlg32.dll");
    if (!hComdlg) return "";
    typedef BOOL(WINAPI *GetSaveFileNameA_Func)(LPOPENFILENAMEA);
    GetSaveFileNameA_Func getSaveFileNameA = (GetSaveFileNameA_Func)GetProcAddress(hComdlg, "GetSaveFileNameA");
    
    if (getSaveFileNameA) {
        char filename[MAX_PATH] = {0};
        OPENFILENAMEA ofn;
        ZeroMemory(&ofn, sizeof(ofn));
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = NULL;
        ofn.lpstrFilter = "Text Files\0*.txt\0All Files\0*.*\0";
        ofn.lpstrFile = filename;
        ofn.nMaxFile = MAX_PATH;
        ofn.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;
        ofn.lpstrDefExt = "txt";
        if (getSaveFileNameA(&ofn)) {
            FreeLibrary(hComdlg);
            return std::string(filename);
        }
    }
    FreeLibrary(hComdlg);
    return "";
}
#else
std::string openFileDialog() { return "circuit_design.txt"; }
std::string saveFileDialog() { return "circuit_design.txt"; }
#endif

const double PI = 3.14159265358979323846;

class Pin {
public:
    double voltage = 0.0;
    std::vector<Pin*> connections;

    void connect(Pin* other) {
        if (std::find(connections.begin(), connections.end(), other) == connections.end()) {
            connections.push_back(other);
            other->connections.push_back(this);
        }
    }
};

struct SolveNet {
    double v = 0.0;
    double sum_g = 0.0;
    double sum_i = 0.0;
    bool fixed = false;
    double fixed_v = 0.0;
    int drivers = 0;
};

std::string formatUnit(double val, const std::string& unit) {
    if (std::isnan(val) || std::isinf(val)) return "0.00 " + unit;
    double absVal = std::abs(val);
    std::stringstream ss;
    ss << std::fixed << std::setprecision(2);

    if (absVal < 1e-6) return "0.00 " + unit;
    if (absVal < 1e-3) { ss << (val * 1e6); return ss.str() + " u" + unit; }
    if (absVal < 1.0)  { ss << (val * 1e3); return ss.str() + " m" + unit; }
    if (absVal >= 1000.0) { ss << (val / 1e3); return ss.str() + " k" + unit; }
    
    ss << val;
    return ss.str() + " " + unit;
}

struct ComponentDef {
    std::string name;
    std::string desc;
    sf::Vector2f size;
};

const std::vector<ComponentDef> COMPONENT_DATABASE = {
    {"AND", "Analog 5V AND Gate", {100.f, 60.f}},
    {"OR", "Analog 5V OR Gate", {100.f, 60.f}},
    {"NOT", "Analog 5V Inverter", {90.f, 50.f}},
    {"NAND", "Analog 5V NAND Gate", {100.f, 60.f}},
    {"NOR", "Analog 5V NOR Gate", {100.f, 60.f}},
    {"XOR", "Analog 5V XOR Gate", {100.f, 60.f}},
    {"XNOR", "Analog 5V XNOR Gate", {100.f, 60.f}},
    {"OP-AMP", "LM741 Operational Amplifier", {80.f, 80.f}},
    {"555-TIMER", "555 Oscillator/Timer IC", {80.f, 100.f}},
    {"RESISTOR", "Basic Resistor (Ohms)", {80.f, 40.f}},
    {"CAPACITOR", "Basic Capacitor (Farads)", {80.f, 40.f}},
    {"INDUCTOR", "Basic Inductor (Henries)", {80.f, 40.f}},
    {"DIODE", "P-N Junction Diode", {60.f, 40.f}},
    {"ZENER", "Zener Diode (editable Vz)", {60.f, 40.f}},
    {"NPN", "NPN BJT Transistor", {60.f, 60.f}},
    {"PNP", "PNP BJT Transistor", {60.f, 60.f}},
    {"N-EMOS", "N-Channel MOSFET", {60.f, 60.f}},
    {"P-EMOS", "P-Channel MOSFET", {60.f, 60.f}},
    {"JFET-N", "N-Channel JFET", {60.f, 60.f}},
    {"V-SRC", "DC Voltage Source", {60.f, 40.f}},
    {"AC-SINE", "Sine Wave Generator (V Hz)", {80.f, 40.f}},
    {"AC-SQUARE", "Square Wave Gen (V Hz)", {80.f, 40.f}},
    {"GND", "Ground (0V Reference)", {40.f, 40.f}},
    {"VOLTMETER", "Measures Potential Diff", {50.f, 50.f}},
    {"AMMETER", "Measures Current", {50.f, 50.f}},
    {"OSCILLOSCOPE", "Scope (Edit VMax TimeDiv_ms)", {200.f, 100.f}}
};

class SimComponent {
public:
    std::vector<std::unique_ptr<Pin>> pins;
    std::string type;
    int id;
    
    double target_voltage = 0.0; 
    std::string input_buffer = "";
    bool is_selected = false;
    
    double state_v = 0.0;
    double state_i = 0.0;
    bool timer_state = false;
    std::deque<float> osc_history;

    sf::Vector2f position;
    sf::Vector2f size;

    SimComponent(std::string t, int comp_id, sf::Vector2f spawn_pos, sf::Vector2f sz) 
        : type(std::move(t)), id(comp_id), position(spawn_pos), size(sz) {
        
        int num_pins = 2;
        if (type == "555-TIMER") num_pins = 8;
        else if (type == "OP-AMP") num_pins = 5;
        else if ((isGate() && type != "NOT") || type == "NPN" || type == "PNP" || type == "N-EMOS" || type == "P-EMOS" || type == "JFET-N") num_pins = 3;
        else if (type == "V-SRC" || type == "GND" || type == "AC-SINE" || type == "AC-SQUARE") num_pins = 1;

        for (int i = 0; i < num_pins; ++i) pins.push_back(std::make_unique<Pin>());

        if (type == "RESISTOR") input_buffer = "1000";
        else if (type == "CAPACITOR") input_buffer = "0.000001";
        else if (type == "INDUCTOR") input_buffer = "0.001";
        else if (type == "V-SRC") input_buffer = "5.0";
        else if (type == "AC-SINE" || type == "AC-SQUARE") input_buffer = "5 1"; 
        else if (type == "ZENER") input_buffer = "5.1";
        else if (type == "OSCILLOSCOPE") input_buffer = "15 10"; // Default: 15V VMax, 10ms per Division
    }

    bool isEditable() const { 
        return type == "V-SRC" || type == "AC-SINE" || type == "AC-SQUARE" || type == "RESISTOR" || type == "CAPACITOR" || type == "INDUCTOR" || type == "ZENER" || type == "OSCILLOSCOPE"; 
    }
    bool isGate() const { 
        return type == "AND" || type == "OR" || type == "NOT" || type == "NAND" || type == "NOR" || type == "XOR" || type == "XNOR"; 
    }

    std::vector<Pin*> getActivePins() const {
        std::vector<Pin*> active;
        for (auto& p : pins) active.push_back(p.get());
        return active;
    }

    void disconnectAll() {
        for (Pin* p : getActivePins()) {
            for (Pin* connected : p->connections) {
                connected->connections.erase(
                    std::remove(connected->connections.begin(), connected->connections.end(), p),
                    connected->connections.end()
                );
            }
            p->connections.clear();
        }
    }

    sf::Vector2f getPinPos(const Pin* p) const {
        int idx = -1;
        for (std::size_t i = 0; i < pins.size(); ++i) {
            if (pins[i].get() == p) { idx = static_cast<int>(i); break; }
        }
        if (idx == -1) return position;

        if (type == "555-TIMER") {
            if(idx == 0) return position + sf::Vector2f(0, size.y * 0.2f);
            if(idx == 1) return position + sf::Vector2f(0, size.y * 0.4f);
            if(idx == 2) return position + sf::Vector2f(0, size.y * 0.6f);
            if(idx == 3) return position + sf::Vector2f(0, size.y * 0.8f);
            if(idx == 4) return position + sf::Vector2f(size.x, size.y * 0.8f);
            if(idx == 5) return position + sf::Vector2f(size.x, size.y * 0.6f);
            if(idx == 6) return position + sf::Vector2f(size.x, size.y * 0.4f);
            if(idx == 7) return position + sf::Vector2f(size.x, size.y * 0.2f);
        }
        else if (type == "OP-AMP") {
            if(idx == 0) return position + sf::Vector2f(0, size.y * 0.75f);
            if(idx == 1) return position + sf::Vector2f(0, size.y * 0.25f);
            if(idx == 2) return position + sf::Vector2f(size.x * 0.5f, 0);
            if(idx == 3) return position + sf::Vector2f(size.x * 0.5f, size.y);
            if(idx == 4) return position + sf::Vector2f(size.x, size.y * 0.5f);
        }
        else if (type == "OSCILLOSCOPE") {
            if(idx == 0) return position + sf::Vector2f(0, size.y * 0.25f);
            if(idx == 1) return position + sf::Vector2f(0, size.y * 0.75f);
        }
        else if (type == "GND") return position + sf::Vector2f(size.x/2, 0);
        else if (type == "V-SRC" || type == "AC-SINE" || type == "AC-SQUARE") return position + sf::Vector2f(size.x, size.y/2);
        else if (type == "NOT") {
            if(idx == 0) return position + sf::Vector2f(0, size.y/2);
            if(idx == 1) return position + sf::Vector2f(size.x, size.y/2);
        }
        else if (isGate()) {
            if(idx == 0) return position + sf::Vector2f(0, size.y * 0.3f);
            if(idx == 1) return position + sf::Vector2f(0, size.y * 0.7f);
            if(idx == 2) return position + sf::Vector2f(size.x, size.y * 0.5f);
        }
        else if (type == "NPN" || type == "PNP" || type == "N-EMOS" || type == "P-EMOS" || type == "JFET-N") {
            if(idx == 0) return position + sf::Vector2f(0, size.y/2);
            if(idx == 1) return position + sf::Vector2f(size.x/2, 0);
            if(idx == 2) return position + sf::Vector2f(size.x/2, size.y);
        }
        else { 
            if(idx == 0) return position + sf::Vector2f(0, size.y/2);
            if(idx == 1) return position + sf::Vector2f(size.x, size.y/2);
        }
        return position;
    }

    void evaluateTargets(double time, double dt) {
        if (isGate()) {
            double vA = pins[0]->voltage;
            double vB = (type != "NOT") ? pins[1]->voltage : 0.0;
            
            if (type == "AND") target_voltage = (vA > 2.5 && vB > 2.5) ? 5.0 : 0.0;
            else if (type == "OR") target_voltage = (vA > 2.5 || vB > 2.5) ? 5.0 : 0.0;
            else if (type == "NAND") target_voltage = !(vA > 2.5 && vB > 2.5) ? 5.0 : 0.0;
            else if (type == "NOR") target_voltage = !(vA > 2.5 || vB > 2.5) ? 5.0 : 0.0;
            else if (type == "XOR") target_voltage = ((vA > 2.5) != (vB > 2.5)) ? 5.0 : 0.0;
            else if (type == "XNOR") target_voltage = ((vA > 2.5) == (vB > 2.5)) ? 5.0 : 0.0;
            else if (type == "NOT") target_voltage = (vA < 2.5) ? 5.0 : 0.0;
        }
        else if (type == "555-TIMER") {
            double v_trig = pins[1]->voltage;
            double v_rst = pins[3]->voltage;
            double v_thr = pins[5]->voltage;
            double v_cc = pins[7]->voltage;
            
            if (v_rst < 0.7) timer_state = false;
            else if (v_trig < v_cc / 3.0) timer_state = true;
            else if (v_thr > 2.0 * v_cc / 3.0) timer_state = false;
            
            target_voltage = timer_state ? v_cc : 0.0;
        }
        else if (type == "OP-AMP") {
            double vInP = pins[0]->voltage;
            double vInN = pins[1]->voltage;
            double vP = pins[2]->voltage;
            double vN = pins[3]->voltage;
            
            double error = vInP - vInN;
            
            static int noise_seed = 12345;
            noise_seed = (noise_seed * 1103515245 + 12345) & 0x7fffffff;
            double noise = ((noise_seed / (double)0x7fffffff) - 0.5) * 1e-5; 
            
            target_voltage += (error + noise) * 10000.0 * dt; 
            
            if (target_voltage > vP) target_voltage = vP;
            if (target_voltage < vN) target_voltage = vN;
        }
        else if (type == "V-SRC") {
            try { target_voltage = std::stod(input_buffer.empty() ? "0" : input_buffer); }
            catch(...) { target_voltage = 0.0; }
        }
        else if (type == "AC-SINE" || type == "AC-SQUARE") {
            double v = 5.0, f = 1.0;
            std::stringstream ss(input_buffer);
            ss >> v >> f;
            if (ss.fail()) { v = 5.0; f = 1.0; }
            if (f <= 0.001) f = 0.001;

            if (type == "AC-SINE") {
                target_voltage = v * std::sin(2.0 * PI * f * time);
            } else {
                target_voltage = std::sin(2.0 * PI * f * time) >= 0 ? v : -v;
            }
        }
    }

    void applyBranchCurrents(std::unordered_map<Pin*, SolveNet*>& net_map, double dt) {
        if (type == "RESISTOR" || type == "AMMETER") {
            double r = 1000.0;
            if (type == "RESISTOR") { try { r = std::stod(input_buffer.empty()?"0":input_buffer); } catch(...) {} }
            if (type == "AMMETER") r = 0.001;
            if (r < 0.001) r = 0.001; 
            
            double g = 1.0 / r;
            auto n0 = net_map[pins[0].get()];
            auto n1 = net_map[pins[1].get()];
            
            n0->sum_g += g; n0->sum_i += n1->v * g;
            n1->sum_g += g; n1->sum_i += n0->v * g;
        }
        else if (type == "CAPACITOR") {
            double c = 1e-6;
            try { c = std::stod(input_buffer.empty()?"0":input_buffer); } catch(...) {}
            if (c < 1e-12) c = 1e-12;
            
            double g = c / dt;
            double i_src = c * state_v / dt; 
            
            auto n0 = net_map[pins[0].get()];
            auto n1 = net_map[pins[1].get()];
            
            n0->sum_g += g; n0->sum_i += n1->v * g + i_src;
            n1->sum_g += g; n1->sum_i += n0->v * g - i_src;
        }
        else if (type == "INDUCTOR") {
            double l = 0.001;
            try { l = std::stod(input_buffer.empty()?"0":input_buffer); } catch(...) {}
            if (l < 1e-9) l = 1e-9;
            
            double g = dt / l;
            double i_src = state_i; 
            
            auto n0 = net_map[pins[0].get()];
            auto n1 = net_map[pins[1].get()];
            
            n0->sum_g += g; n0->sum_i += n1->v * g - i_src;
            n1->sum_g += g; n1->sum_i += n0->v * g + i_src;
        }
        else if (type == "DIODE" || type == "ZENER") {
            auto n0 = net_map[pins[0].get()];
            auto n1 = net_map[pins[1].get()];
            double vD = n0->v - n1->v;
            
            double g = 1e-9;
            if (vD > 0.5) {
                g = std::pow((vD - 0.5) * 5.0, 2.0); 
            }
            else if (type == "ZENER") {
                double vz = 5.1;
                try { vz = std::stod(input_buffer.empty() ? "0" : input_buffer); } catch(...) {}
                if (vD < -vz) {
                    g = std::pow((-vD - vz) * 5.0, 2.0);
                }
            }
            
            if (g > 10.0) g = 10.0; 
            
            n0->sum_g += g; n0->sum_i += n1->v * g;
            n1->sum_g += g; n1->sum_i += n0->v * g;
        }
        else if (type == "NPN" || type == "PNP" || type == "N-EMOS" || type == "P-EMOS" || type == "JFET-N") {
            auto n0 = net_map[pins[0].get()]; 
            auto n1 = net_map[pins[1].get()]; 
            auto n2 = net_map[pins[2].get()]; 
            
            double g = 1e-9; 
            if (type == "NPN") {
                double vbe = n0->v - n2->v;
                if (vbe > 0.5) g = std::pow((vbe - 0.5) * 10.0, 2.0);
            }
            else if (type == "PNP") {
                double veb = n1->v - n0->v;
                if (veb > 0.5) g = std::pow((veb - 0.5) * 10.0, 2.0);
            }
            else if (type == "N-EMOS") {
                double vgs = n0->v - n2->v;
                if (vgs > 1.5) g = std::pow((vgs - 1.5) * 5.0, 2.0);
            }
            else if (type == "P-EMOS") {
                double vsg = n1->v - n0->v;
                if (vsg > 1.5) g = std::pow((vsg - 1.5) * 5.0, 2.0);
            }
            else if (type == "JFET-N") {
                double vgs = n0->v - n2->v;
                if (vgs > -2.0) g = 0.05 * std::pow((vgs + 2.0), 2.0);
            }
            
            if (g > 20.0) g = 20.0; 
            
            n1->sum_g += g; n1->sum_i += n2->v * g;
            n2->sum_g += g; n2->sum_i += n1->v * g;
        }
        else if (type == "555-TIMER") {
            if (!timer_state) {
                auto n_dis = net_map[pins[6].get()];
                auto n_gnd = net_map[pins[0].get()];
                double g = 1.0;
                n_dis->sum_g += g; n_dis->sum_i += n_gnd->v * g;
                n_gnd->sum_g += g; n_gnd->sum_i += n_dis->v * g;
            }
            auto n_vcc = net_map[pins[7].get()];
            auto n_gnd = net_map[pins[0].get()];
            double g_div = 1.0 / 15000.0;
            n_vcc->sum_g += g_div; n_vcc->sum_i += n_gnd->v * g_div;
            n_gnd->sum_g += g_div; n_gnd->sum_i += n_vcc->v * g_div;
        }
    }

    void updateState(std::unordered_map<Pin*, SolveNet*>& net_map, double dt) {
        if (type == "CAPACITOR") {
            auto n0 = net_map[pins[0].get()];
            auto n1 = net_map[pins[1].get()];
            state_v = n0->v - n1->v;
        }
        else if (type == "INDUCTOR") {
            double l = 0.001;
            try { l = std::stod(input_buffer.empty()?"0":input_buffer); } catch(...) {}
            if (l < 1e-9) l = 1e-9;
            
            auto n0 = net_map[pins[0].get()];
            auto n1 = net_map[pins[1].get()];
            state_i += (n0->v - n1->v) * (dt / l);
        }
        else if (type == "OSCILLOSCOPE") {
            auto n0 = net_map[pins[0].get()];
            auto n1 = net_map[pins[1].get()];
            
            double v0 = n0 ? n0->v : 0.0;
            double v1 = (n1 && !pins[1]->connections.empty()) ? n1->v : 0.0;
            double diff = v0 - v1;
            
            if (std::isnan(diff) || std::isinf(diff)) diff = 0.0;
            osc_history.push_back(static_cast<float>(diff));
            if (osc_history.size() > 3000) osc_history.pop_front();
        }
    }

    bool checkCollision(sf::Vector2f mousePos) const {
        return (mousePos.x >= position.x && mousePos.x <= position.x + size.x &&
                mousePos.y >= position.y && mousePos.y <= position.y + size.y);
    }

    void draw(sf::RenderWindow& window, const sf::Font& font) {
        sf::Color bodyColor(220, 225, 235);
        sf::Color outlineColor = is_selected ? sf::Color(255, 50, 50) : sf::Color(30, 30, 30);
        float thick = is_selected ? 3.f : 2.f;

        if (type == "V-SRC" || type == "AC-SINE" || type == "AC-SQUARE" || type == "RESISTOR" || type == "CAPACITOR" || type == "INDUCTOR") {
            sf::RectangleShape body(size);
            body.setPosition(position);
            body.setFillColor(is_selected ? sf::Color(255, 220, 220) : sf::Color(255, 250, 235));
            body.setOutlineColor(outlineColor);
            body.setOutlineThickness(thick);
            window.draw(body);

            std::string valStr = input_buffer + (is_selected ? "_" : "");
            std::string unit = " V";
            if (type == "RESISTOR") unit = " Ohm";
            if (type == "CAPACITOR") unit = " F";
            if (type == "INDUCTOR") unit = " H";
            
            if (!is_selected) {
                if (type == "AC-SINE" || type == "AC-SQUARE") {
                    std::stringstream ss(input_buffer);
                    double v = 5.0, f = 1.0;
                    ss >> v >> f;
                    valStr = formatUnit(v, "V") + " " + formatUnit(f, "Hz");
                } else {
                    try { valStr = formatUnit(std::stod(input_buffer.empty() ? "0" : input_buffer), unit.substr(1)); } 
                    catch(...) { valStr = "Error"; }
                }
            } else if (type != "AC-SINE" && type != "AC-SQUARE") { 
                valStr += unit; 
            }

            std::string displayName = type;
            if (type == "RESISTOR") displayName = "RES";
            else if (type == "CAPACITOR") displayName = "CAP";
            else if (type == "INDUCTOR") displayName = "IND";
            else if (type == "AC-SINE") displayName = "SINEWAVE";
            else if (type == "AC-SQUARE") displayName = "SQRWAVE";
            
            sf::Text label(font, displayName + "\n" + valStr, 11);
            label.setPosition(position + sf::Vector2f(5.f, 4.f));
            label.setFillColor((type == "AC-SINE" || type == "AC-SQUARE") ? sf::Color(200, 0, 0) : sf::Color::Black);
            window.draw(label);
        }
        else if (type == "OSCILLOSCOPE") {
            double max_v = 15.0;
            double time_div = 10.0; // Time division in milliseconds per division
            std::stringstream ss(input_buffer);
            ss >> max_v >> time_div;
            if (ss.fail() || max_v < 0.1) max_v = 15.0;
            if (time_div < 0.1) time_div = 10.0;

            sf::RectangleShape body(size);
            body.setPosition(position);
            body.setFillColor(is_selected ? sf::Color(20, 30, 25) : sf::Color(10, 20, 15)); 
            body.setOutlineColor(is_selected ? sf::Color(255, 60, 60) : outlineColor);
            body.setOutlineThickness(thick);
            window.draw(body);

            // Draw CRT Scope Grid (10 horizontal x 8 vertical divisions)
            for (int i = 1; i < 10; ++i) {
                float gx = position.x + (size.x / 10.f) * i;
                sf::Vertex line[] = {
                    sf::Vertex{sf::Vector2f(gx, position.y), sf::Color(25, 55, 35)},
                    sf::Vertex{sf::Vector2f(gx, position.y + size.y), sf::Color(25, 55, 35)}
                };
                window.draw(line, 2, sf::PrimitiveType::Lines);
            }
            for (int i = 1; i < 8; ++i) {
                float gy = position.y + (size.y / 8.f) * i;
                sf::Vertex line[] = {
                    sf::Vertex{sf::Vector2f(position.x, gy), sf::Color(25, 55, 35)},
                    sf::Vertex{sf::Vector2f(position.x + size.x, gy), sf::Color(25, 55, 35)}
                };
                window.draw(line, 2, sf::PrimitiveType::Lines);
            }

            // Zero Center Line
            sf::Vertex centerLine[] = {
                sf::Vertex{sf::Vector2f(position.x, position.y + size.y / 2.f), sf::Color(50, 110, 70)},
                sf::Vertex{sf::Vector2f(position.x + size.x, position.y + size.y / 2.f), sf::Color(50, 110, 70)}
            };
            window.draw(centerLine, 2, sf::PrimitiveType::Lines);

            if (!osc_history.empty()) {
                size_t req_pts = static_cast<size_t>((10.0 * time_div) / 1.6667);
                if (req_pts < 10) req_pts = 10;

                size_t start_idx = 0;
                if (osc_history.size() > req_pts) {
                    size_t search_start = osc_history.size() - req_pts;
                    bool found_trigger = false;
                    size_t search_limit = (search_start > 150) ? (search_start - 150) : 0;
                    for (size_t i = search_limit; i + 1 < search_start + 150 && i + 1 < osc_history.size(); ++i) {
                        if (osc_history[i] <= 0.f && osc_history[i+1] > 0.f) {
                            start_idx = i;
                            found_trigger = true;
                            break;
                        }
                    }
                    if (!found_trigger) start_idx = osc_history.size() - req_pts;
                }

                size_t points_to_render = std::min(req_pts, osc_history.size() - start_idx);

                if (points_to_render > 1) {
                    sf::VertexArray wave(sf::PrimitiveType::LineStrip);
                    sf::VertexArray waveGlow(sf::PrimitiveType::LineStrip);

                    for (size_t i = 0; i < points_to_render; ++i) {
                        float px = position.x + (static_cast<float>(i) / static_cast<float>(points_to_render - 1)) * size.x;
                        float val = osc_history[start_idx + i];
                        if (std::isnan(val) || std::isinf(val)) val = 0.f;

                        float py = position.y + size.y / 2.f - (val / static_cast<float>(max_v)) * (size.y / 2.f - 4.f);
                        py = std::clamp(py, position.y + 2.f, position.y + size.y - 2.f);

                        wave.append(sf::Vertex{sf::Vector2f(px, py), sf::Color(50, 255, 120)});
                        waveGlow.append(sf::Vertex{sf::Vector2f(px, py + 1.f), sf::Color(30, 180, 80, 120)});
                    }
                    window.draw(waveGlow);
                    window.draw(wave);
                }
            }

            sf::Text title(font, "OSCILLOSCOPE", 11);
            title.setPosition(position + sf::Vector2f(5.f, 3.f));
            title.setFillColor(sf::Color(80, 255, 150));
            window.draw(title);

            std::string settingsStr = is_selected 
                ? ("Edit: " + input_buffer + "_") 
                : ("VMax: " + formatUnit(max_v, "V") + " | T/Div: " + formatUnit(time_div * 1e-3, "s"));
            sf::Text settingsText(font, settingsStr, 10);
            settingsText.setPosition(position + sf::Vector2f(5.f, 18.f));
            settingsText.setFillColor(is_selected ? sf::Color(255, 255, 100) : sf::Color(160, 230, 180));
            window.draw(settingsText);

            double v0 = pins[0]->voltage;
            double v1 = !pins[1]->connections.empty() ? pins[1]->voltage : 0.0;
            std::string readStr = formatUnit(v0 - v1, "V");
            sf::Text readout(font, readStr, 11);
            readout.setPosition(position + sf::Vector2f(size.x - 70.f, size.y - 18.f));
            readout.setFillColor(sf::Color(80, 255, 150));
            window.draw(readout);
        }
        else if (type == "OP-AMP") {
            sf::ConvexShape tri;
            tri.setPointCount(3);
            tri.setPoint(0, position + sf::Vector2f(0, 0));
            tri.setPoint(1, position + sf::Vector2f(size.x, size.y/2));
            tri.setPoint(2, position + sf::Vector2f(0, size.y));
            tri.setFillColor(sf::Color(250, 240, 230));
            tri.setOutlineColor(outlineColor);
            tri.setOutlineThickness(thick);
            window.draw(tri);

            sf::Text lInP(font, "+", 14); lInP.setPosition(position + sf::Vector2f(5, size.y * 0.75f - 10)); lInP.setFillColor(sf::Color::Black); window.draw(lInP);
            sf::Text lInN(font, "-", 14); lInN.setPosition(position + sf::Vector2f(5, size.y * 0.25f - 10)); lInN.setFillColor(sf::Color::Black); window.draw(lInN);
            sf::Text title(font, "741", 14); title.setPosition(position + sf::Vector2f(size.x * 0.35f, size.y/2 - 10)); title.setFillColor(sf::Color(100,100,100)); window.draw(title);
        }
        else if (type == "555-TIMER") {
            sf::RectangleShape body(size);
            body.setPosition(position);
            body.setFillColor(sf::Color(40, 40, 40));
            body.setOutlineColor(outlineColor);
            body.setOutlineThickness(thick);
            window.draw(body);

            sf::CircleShape notch(5.f);
            notch.setPosition({position.x + size.x/2 - 5.f, position.y - 2.f});
            notch.setFillColor(sf::Color(20, 20, 20));
            window.draw(notch);

            sf::Text title(font, "NE555", 14);
            title.setPosition(position + sf::Vector2f(size.x/2 - 20.f, size.y/2 - 10.f));
            title.setFillColor(sf::Color::White);
            window.draw(title);
        }
        else if (type == "DIODE" || type == "ZENER") {
            sf::VertexArray lines(sf::PrimitiveType::Lines);
            lines.append(sf::Vertex{sf::Vector2f(position.x, position.y + size.y/2), outlineColor});
            lines.append(sf::Vertex{sf::Vector2f(position.x + size.x, position.y + size.y/2), outlineColor});
            
            sf::ConvexShape tri(3);
            tri.setPoint(0, {position.x + 15.f, position.y + 10.f});
            tri.setPoint(1, {position.x + 15.f, position.y + size.y - 10.f});
            tri.setPoint(2, {position.x + size.x - 15.f, position.y + size.y/2});
            tri.setFillColor(sf::Color(200, 50, 50));
            tri.setOutlineColor(outlineColor);
            tri.setOutlineThickness(thick);
            window.draw(tri);

            sf::RectangleShape bar(sf::Vector2f(thick, size.y - 20.f));
            bar.setPosition({position.x + size.x - 15.f, position.y + 10.f});
            bar.setFillColor(outlineColor);
            window.draw(bar);
            window.draw(lines);
        }
        else if (type == "GND") {
            sf::VertexArray lines(sf::PrimitiveType::Lines);
            sf::Vector2f c = position + sf::Vector2f(size.x / 2.f, 0.f);
            
            lines.append(sf::Vertex{sf::Vector2f(c.x, c.y), outlineColor});
            lines.append(sf::Vertex{sf::Vector2f(c.x, c.y + 15.f), outlineColor});
            lines.append(sf::Vertex{sf::Vector2f(c.x - 15.f, c.y + 15.f), outlineColor});
            lines.append(sf::Vertex{sf::Vector2f(c.x + 15.f, c.y + 15.f), outlineColor});
            lines.append(sf::Vertex{sf::Vector2f(c.x - 10.f, c.y + 22.f), outlineColor});
            lines.append(sf::Vertex{sf::Vector2f(c.x + 10.f, c.y + 22.f), outlineColor});
            window.draw(lines);
        }
        else if (type == "VOLTMETER" || type == "AMMETER") {
            sf::CircleShape meter(size.x / 2.f);
            meter.setPosition(position);
            meter.setFillColor(sf::Color(250, 250, 250));
            meter.setOutlineColor(outlineColor);
            meter.setOutlineThickness(thick);
            window.draw(meter);

            double v0 = pins[0]->voltage;
            double v1 = !pins[1]->connections.empty() ? pins[1]->voltage : 0.0;

            std::string display = (type == "VOLTMETER") 
                ? formatUnit(v0 - v1, "V")
                : formatUnit((v0 - v1) / 0.001, "A");

            sf::Text label(font, display, 12);
            label.setPosition(position + sf::Vector2f(10.f, size.y / 2.f - 6.f));
            label.setFillColor(sf::Color::Blue);
            window.draw(label);
        }
        else {
            sf::RectangleShape body(size);
            body.setPosition(position);
            body.setFillColor(bodyColor);
            body.setOutlineColor(outlineColor);
            body.setOutlineThickness(thick);
            window.draw(body);
            
            sf::Text label(font, type, 14);
            label.setPosition(position + sf::Vector2f(10.f, size.y / 2.f - 8.f));
            label.setFillColor(sf::Color::Black);
            window.draw(label);
        }

        for (Pin* p : getActivePins()) {
            sf::CircleShape dot(7.f);
            dot.setOrigin({7.f, 7.f});
            dot.setPosition(getPinPos(p));
            dot.setFillColor(sf::Color::Black);
            window.draw(dot);
        }
    }
};

struct CompSnapshot {
    int id;
    std::string type;
    sf::Vector2f pos;
    std::string input_buffer;
};

struct WireSnapshot {
    int compA_id, pinA_idx, compB_id, pinB_idx;
};

struct CircuitState {
    std::vector<CompSnapshot> comps;
    std::vector<WireSnapshot> wires;
};

void drawThickLine(sf::RenderWindow& window, sf::Vector2f p1, sf::Vector2f p2, float thickness, sf::Color color) {
    sf::Vector2f dir = p2 - p1;
    float length = std::hypot(dir.x, dir.y);
    if (length == 0.f) return;
    
    dir /= length;
    sf::Vector2f normal(-dir.y, dir.x);
    sf::Vector2f offset = normal * (thickness / 2.f);
    
    sf::VertexArray quad(sf::PrimitiveType::TriangleStrip, 4);
    quad[0].position = p1 + offset;
    quad[1].position = p1 - offset;
    quad[2].position = p2 + offset;
    quad[3].position = p2 - offset;
    for(int i = 0; i < 4; ++i) quad[i].color = color;
    
    window.draw(quad);
}

std::vector<std::pair<sf::Vector2f, sf::Vector2f>> getOrthogonalSegments(sf::Vector2f p1, sf::Vector2f p2) {
    std::vector<std::pair<sf::Vector2f, sf::Vector2f>> segments;
    float midX = (p1.x + p2.x) / 2.f;
    sf::Vector2f m1(midX, p1.y);
    sf::Vector2f m2(midX, p2.y);
    
    segments.push_back({p1, m1});
    segments.push_back({m1, m2});
    segments.push_back({m2, p2});
    return segments;
}

class SimulatorUI {
private:
    std::vector<std::unique_ptr<SimComponent>> components;
    SimComponent* draggedComponent = nullptr;
    sf::Vector2f dragOffset;
    Pin* selectedSourcePin = nullptr;

    bool isSearching = false;
    std::string searchQuery = "";
    std::string preEditText = "";
    const ComponentDef* componentToPlace = nullptr;

    std::vector<CircuitState> undo_stack;
    std::vector<CircuitState> redo_stack;
    int global_comp_id = 1;
    CircuitState preDragState;
    bool isDragging = false;
    bool hasDraggedMoved = false;
    
    double global_time = 0.0;

    CircuitState saveState() {
        CircuitState state;
        for (const auto& c : components) {
            state.comps.push_back(CompSnapshot{c->id, c->type, c->position, c->input_buffer});
        }
        for (const auto& c : components) {
            auto pins = c->getActivePins();
            for (std::size_t i = 0; i < pins.size(); ++i) {
                for (Pin* conn : pins[i]->connections) {
                    int compB_id = -1, pinB_idx = -1;
                    for (const auto& cb : components) {
                        auto bpins = cb->getActivePins();
                        for (std::size_t j = 0; j < bpins.size(); ++j) {
                            if (bpins[j] == conn) {
                                compB_id = cb->id;
                                pinB_idx = static_cast<int>(j);
                                break;
                            }
                        }
                        if (compB_id != -1) break;
                    }
                    if (c->id < compB_id || (c->id == compB_id && static_cast<int>(i) < pinB_idx)) {
                        state.wires.push_back(WireSnapshot{c->id, static_cast<int>(i), compB_id, pinB_idx});
                    }
                }
            }
        }
        return state;
    }

    void loadState(const CircuitState& state) {
        components.clear();
        selectedSourcePin = nullptr;
        draggedComponent = nullptr;
        isDragging = false;
        
        int max_id = global_comp_id;
        for (const auto& cs : state.comps) {
            sf::Vector2f size{50.f, 50.f};
            for (const auto& def : COMPONENT_DATABASE) {
                if (def.name == cs.type) { size = def.size; break; }
            }
            auto comp = std::make_unique<SimComponent>(cs.type, cs.id, cs.pos, size);
            comp->input_buffer = cs.input_buffer;
            components.push_back(std::move(comp));
            if (cs.id > max_id) max_id = cs.id;
        }
        global_comp_id = max_id + 1;

        for (const auto& ws : state.wires) {
            SimComponent* cA = nullptr;
            SimComponent* cB = nullptr;
            for (auto& c : components) {
                if (c->id == ws.compA_id) cA = c.get();
                if (c->id == ws.compB_id) cB = c.get();
            }
            if (cA && cB) {
                auto pinsA = cA->getActivePins();
                auto pinsB = cB->getActivePins();
                if (ws.pinA_idx >= 0 && ws.pinA_idx < static_cast<int>(pinsA.size()) && 
                    ws.pinB_idx >= 0 && ws.pinB_idx < static_cast<int>(pinsB.size())) {
                    pinsA[ws.pinA_idx]->connect(pinsB[ws.pinB_idx]);
                }
            }
        }
    }

    void saveUndoState() {
        undo_stack.push_back(saveState());
        redo_stack.clear();
    }

    void undo() {
        if (!undo_stack.empty()) {
            redo_stack.push_back(saveState());
            loadState(undo_stack.back());
            undo_stack.pop_back();
        }
    }

    void redo() {
        if (!redo_stack.empty()) {
            undo_stack.push_back(saveState());
            loadState(redo_stack.back());
            redo_stack.pop_back();
        }
    }

    void saveToFile(const std::string& filename) {
        CircuitState state = saveState();
        std::ofstream out(filename);
        if (!out) return;
        
        out << state.comps.size() << "\n";
        for (const auto& c : state.comps) {
            std::string buf = c.input_buffer.empty() ? "EMPTY" : c.input_buffer;
            std::replace(buf.begin(), buf.end(), ' ', '_'); 
            out << c.id << " " << c.type << " " << c.pos.x << " " << c.pos.y << " " << buf << "\n";
        }
        out << state.wires.size() << "\n";
        for (const auto& w : state.wires) {
            out << w.compA_id << " " << w.pinA_idx << " " << w.compB_id << " " << w.pinB_idx << "\n";
        }
    }

    void loadFromFile(const std::string& filename) {
        std::ifstream in(filename);
        if (!in) return;
        
        CircuitState state;
        size_t comp_count;
        if (!(in >> comp_count)) return;
        
        for (size_t i = 0; i < comp_count; ++i) {
            CompSnapshot cs;
            std::string buf;
            in >> cs.id >> cs.type >> cs.pos.x >> cs.pos.y >> buf;
            std::replace(buf.begin(), buf.end(), '_', ' ');
            cs.input_buffer = (buf == "EMPTY") ? "" : buf;
            state.comps.push_back(cs);
        }
        
        size_t wire_count;
        if (!(in >> wire_count)) return;
        
        for (size_t i = 0; i < wire_count; ++i) {
            WireSnapshot ws;
            in >> ws.compA_id >> ws.pinA_idx >> ws.compB_id >> ws.pinB_idx;
            state.wires.push_back(ws);
        }
        
        saveUndoState();
        loadState(state);
    }

    float getDistanceToLineSegment(sf::Vector2f p, sf::Vector2f a, sf::Vector2f b) const {
        sf::Vector2f ab = b - a;
        sf::Vector2f ap = p - a;
        float lengthSq = ab.x * ab.x + ab.y * ab.y;
        if (lengthSq == 0.f) return std::hypot(ap.x, ap.y);
        float t = std::max(0.f, std::min(1.f, (ap.x * ab.x + ap.y * ab.y) / lengthSq));
        sf::Vector2f proj = a + t * ab;
        return std::hypot(p.x - proj.x, p.y - proj.y);
    }

    Pin* findPinNearMouse(sf::Vector2f mousePos) {
        for (const auto& comp : components) {
            for (Pin* p : comp->getActivePins()) {
                sf::Vector2f pinPos = comp->getPinPos(p);
                if (std::hypot(mousePos.x - pinPos.x, mousePos.y - pinPos.y) < 14.f) return p;
            }
        }
        return nullptr;
    }

    sf::Vector2f getPinScreenPos(Pin* p) const {
        for (const auto& comp : components) {
            for (Pin* active : comp->getActivePins()) {
                if (p == active) return comp->getPinPos(p);
            }
        }
        return {0.f, 0.f};
    }

    bool deleteComponentAt(sf::Vector2f pos) {
        for (auto it = components.rbegin(); it != components.rend(); ++it) {
            if ((*it)->checkCollision(pos)) {
                saveUndoState();
                auto compPtr = it->get();
                if (draggedComponent == compPtr) { draggedComponent = nullptr; isDragging = false; }
                for (Pin* p : compPtr->getActivePins()) {
                    if (selectedSourcePin == p) selectedSourcePin = nullptr;
                }
                compPtr->disconnectAll();
                components.erase(std::next(it).base());
                return true;
            }
        }
        return false;
    }

    bool deleteWireAt(sf::Vector2f pos) {
        for (const auto& comp : components) {
            for (Pin* pA : comp->getActivePins()) {
                for (Pin* pB : pA->connections) {
                    if (pA < pB) {
                        auto segments = getOrthogonalSegments(getPinScreenPos(pA), getPinScreenPos(pB));
                        for (const auto& seg : segments) {
                            if (getDistanceToLineSegment(pos, seg.first, seg.second) < 10.f) {
                                saveUndoState();
                                pA->connections.erase(std::remove(pA->connections.begin(), pA->connections.end(), pB), pA->connections.end());
                                pB->connections.erase(std::remove(pB->connections.begin(), pB->connections.end(), pA), pB->connections.end());
                                return true;
                            }
                        }
                    }
                }
            }
        }
        return false;
    }

    void resolveAnalogNets() {
        int substeps = 10;
        double frame_dt = 1.0 / 60.0;
        double dt = frame_dt / substeps;

        for (int step = 0; step < substeps; ++step) {
            std::unordered_map<Pin*, SolveNet*> pin_net_map;
            std::vector<std::unique_ptr<SolveNet>> nets;
            std::set<Pin*> visited;
            
            for (auto& comp : components) {
                for (auto* pin : comp->getActivePins()) {
                    if (visited.count(pin)) continue;
                    
                    auto net = std::make_unique<SolveNet>();
                    std::vector<Pin*> queue = {pin};
                    visited.insert(pin);
                    
                    while (!queue.empty()) {
                        Pin* curr = queue.back();
                        queue.pop_back();
                        pin_net_map[curr] = net.get();
                        if (curr->voltage != 0.0) net->v = curr->voltage; 
                        
                        for (Pin* neighbor : curr->connections) {
                            if (!visited.count(neighbor)) {
                                visited.insert(neighbor);
                                queue.push_back(neighbor);
                            }
                        }
                    }
                    nets.push_back(std::move(net));
                }
            }

            for (auto& comp : components) {
                comp->evaluateTargets(global_time, dt);
                
                int out_idx = -1;
                if (comp->type == "GND" || comp->type == "V-SRC" || comp->type == "AC-SINE" || comp->type == "AC-SQUARE") out_idx = 0;
                else if (comp->type == "NOT") out_idx = 1;
                else if (comp->isGate()) out_idx = 2;
                else if (comp->type == "OP-AMP") out_idx = 4;
                else if (comp->type == "555-TIMER") out_idx = 2; 
                
                if (out_idx != -1 && out_idx < static_cast<int>(comp->pins.size())) {
                    auto* net = pin_net_map[comp->pins[out_idx].get()];
                    if (net) {
                        if (comp->type == "GND") net->fixed_v += 0.0;
                        else net->fixed_v += comp->target_voltage;
                        net->drivers++;
                        net->fixed = true;
                    }
                }
            }
            
            for (auto& net : nets) {
                if (net->fixed && net->drivers > 0) {
                    net->fixed_v /= net->drivers;
                    net->v = net->fixed_v;
                }
            }
            
            for (int iter = 0; iter < 100; ++iter) {
                for (auto& net : nets) { net->sum_g = 0.0; net->sum_i = 0.0; }
                for (auto& comp : components) comp->applyBranchCurrents(pin_net_map, dt);
                
                for (auto& net : nets) {
                    if (!net->fixed) {
                        if (net->sum_g > 1e-12) net->v = net->sum_i / net->sum_g;
                        else net->v = 0.0; 
                    }
                }
            }
            
            for (auto& comp : components) comp->updateState(pin_net_map, dt);
            for (auto& kv : pin_net_map) kv.first->voltage = kv.second->v;

            global_time += dt;
        }
    }

public:
    void run() {
        sf::RenderWindow window(sf::VideoMode({1920, 1080}), "Circuit simulator");
        window.setFramerateLimit(60);

        sf::Font font;
        if (!font.openFromFile("bin/arial.ttf")) {
            std::cerr << "Warning: Failed to load font from 'bin/arial.ttf'" << std::endl;
        } 

        while (window.isOpen()) {
            sf::Vector2i pixelMousePos = sf::Mouse::getPosition(window);
            sf::Vector2f mousePos(static_cast<float>(pixelMousePos.x), static_cast<float>(pixelMousePos.y));
            bool clickConsumed = false;

            while (const auto event = window.pollEvent()) {
                if (event->is<sf::Event::Closed>()) window.close();

                if (const auto* keyEvent = event->getIf<sf::Event::KeyPressed>()) {
                    bool ctrl = keyEvent->control;
                    bool shift = keyEvent->shift;
                    
                    if (ctrl && keyEvent->code == sf::Keyboard::Key::Z) {
                        if (shift) redo(); else undo();
                    }
                    else if (ctrl && keyEvent->code == sf::Keyboard::Key::Y) redo();
                    else if (ctrl && keyEvent->code == sf::Keyboard::Key::S) {
                        std::string file = saveFileDialog();
                        if (!file.empty()) saveToFile(file);
                    }
                    else if (ctrl && keyEvent->code == sf::Keyboard::Key::O) {
                        std::string file = openFileDialog();
                        if (!file.empty()) loadFromFile(file);
                    }
                    else if (keyEvent->code == sf::Keyboard::Key::Delete || keyEvent->code == sf::Keyboard::Key::Backspace) {
                        bool isTyping = isSearching;
                        for (const auto& comp : components) {
                            if (comp->is_selected && comp->isEditable()) {
                                isTyping = true;
                                break;
                            }
                        }
                        
                        if (!isTyping) {
                            if (!deleteComponentAt(mousePos)) deleteWireAt(mousePos);
                        }
                    }
                }

                if (const auto* textEvent = event->getIf<sf::Event::TextEntered>()) {
                    uint32_t code = textEvent->unicode;
                    bool typedInComp = false;
                    for (auto& comp : components) {
                        if (comp->is_selected && comp->isEditable()) {
                            if (code == 8 && !comp->input_buffer.empty()) {
                                comp->input_buffer.pop_back();
                            } else if ((code >= '0' && code <= '9') || code == '.' || code == '-' || code == 'e' || code == 'E' || code == ' ') {
                                comp->input_buffer += static_cast<char>(code);
                            } else if (code == 13) { 
                                std::string newText = comp->input_buffer;
                                comp->input_buffer = preEditText;
                                saveUndoState();
                                comp->input_buffer = newText;
                                comp->is_selected = false; 
                            }
                            typedInComp = true;
                            break;
                        }
                    }

                    if (!typedInComp && isSearching) {
                        if (code == 8 && !searchQuery.empty()) searchQuery.pop_back();
                        else if (code >= 32 && code < 128) searchQuery += static_cast<char>(code);
                    }
                }

                if (const auto* btn = event->getIf<sf::Event::MouseButtonPressed>()) {
                    for (auto& comp : components) comp->is_selected = false;

                    if (btn->button == sf::Mouse::Button::Left) {
                        sf::FloatRect searchBounds({10.f, 10.f},{ 300.f, 35.f});
                        if (searchBounds.contains(mousePos)) {
                            isSearching = true;
                            clickConsumed = true;
                        }

                        if (isSearching && !searchQuery.empty() && !clickConsumed) {
                            float x = 10.f;
                            float y = 50.f;
                            std::string lq = searchQuery;
                            std::transform(lq.begin(), lq.end(), lq.begin(), ::tolower);

                            for (const auto& def : COMPONENT_DATABASE) {
                                std::string match = def.name + " " + def.desc;
                                std::transform(match.begin(), match.end(), match.begin(), ::tolower);
                                
                                if (lq == "all" || match.find(lq) != std::string::npos) {
                                    if (sf::FloatRect({x, y},{300.f, 40.f}).contains(mousePos)) {
                                        componentToPlace = &def;
                                        isSearching = false;
                                        searchQuery = "";
                                        clickConsumed = true;
                                        break;
                                    }
                                    y += 40.f;
                                    if (y + 40.f > window.getSize().y) { 
                                        y = 50.f;
                                        x += 310.f;
                                    }
                                }
                            }
                        }

                        if (componentToPlace && !clickConsumed) {
                            saveUndoState();
                            components.push_back(std::make_unique<SimComponent>(
                                componentToPlace->name, global_comp_id++, 
                                mousePos - sf::Vector2f(componentToPlace->size.x / 2.f, componentToPlace->size.y / 2.f), 
                                componentToPlace->size
                            ));
                            componentToPlace = nullptr;
                            clickConsumed = true;
                        }

                        if (!clickConsumed) {
                            Pin* clickedPin = findPinNearMouse(mousePos);
                            if (clickedPin) {
                                if (!selectedSourcePin) selectedSourcePin = clickedPin; 
                                else {
                                    if (clickedPin != selectedSourcePin) {
                                        saveUndoState();
                                        selectedSourcePin->connect(clickedPin);
                                    }
                                    selectedSourcePin = nullptr; 
                                }
                            } else {
                                for (auto it = components.rbegin(); it != components.rend(); ++it) {
                                    if ((*it)->checkCollision(mousePos)) {
                                        if ((*it)->isEditable()) {
                                            (*it)->is_selected = true;
                                            preEditText = (*it)->input_buffer;
                                        }
                                        draggedComponent = it->get();
                                        dragOffset = (*it)->position - mousePos;
                                        isDragging = true;
                                        hasDraggedMoved = false;
                                        preDragState = saveState(); 
                                        break;
                                    }
                                }
                            }
                        }
                    } 
                    else if (btn->button == sf::Mouse::Button::Right) {
                        if (!deleteComponentAt(mousePos)) deleteWireAt(mousePos);
                    }
                }

                if (const auto* btnEvent = event->getIf<sf::Event::MouseButtonReleased>()) {
                    if (btnEvent->button == sf::Mouse::Button::Left) {
                        if (isDragging && hasDraggedMoved) {
                            undo_stack.push_back(preDragState);
                            redo_stack.clear();
                        }
                        isDragging = false;
                        draggedComponent = nullptr;
                    }
                }
            }

            if (draggedComponent) {
                sf::Vector2f newPos = mousePos + dragOffset;
                if (draggedComponent->position != newPos) {
                    hasDraggedMoved = true;
                    draggedComponent->position = newPos;
                }
            }
            
            resolveAnalogNets();

            // --- RENDERING ---
            window.clear(sf::Color(240, 245, 240)); 

            sf::VertexArray grid(sf::PrimitiveType::Lines);
            sf::Vector2u winSize = window.getSize();
            for (unsigned int x = 0; x < winSize.x; x += 20) {
                grid.append(sf::Vertex{sf::Vector2f(static_cast<float>(x), 0.f), sf::Color(220, 225, 220)});
                grid.append(sf::Vertex{sf::Vector2f(static_cast<float>(x), static_cast<float>(winSize.y)), sf::Color(220, 225, 220)});
            }
            for (unsigned int y = 0; y < winSize.y; y += 20) {
                grid.append(sf::Vertex{sf::Vector2f(0.f, static_cast<float>(y)), sf::Color(220, 225, 220)});
                grid.append(sf::Vertex{sf::Vector2f(static_cast<float>(winSize.x), static_cast<float>(y)), sf::Color(220, 225, 220)});
            }
            window.draw(grid);

            for (const auto& comp : components) {
                for (Pin* p : comp->getActivePins()) {
                    for (Pin* target : p->connections) {
                        if (p < target) { 
                            sf::Color c = (std::abs(p->voltage) > 0.1) ? sf::Color(220, 60, 40) : sf::Color(120, 40, 30);
                            auto segments = getOrthogonalSegments(getPinScreenPos(p), getPinScreenPos(target));
                            for (const auto& seg : segments) {
                                drawThickLine(window, seg.first, seg.second, 4.f, c);
                            }
                        }
                    }
                }
            }

            if (selectedSourcePin) {
                auto segments = getOrthogonalSegments(getPinScreenPos(selectedSourcePin), mousePos);
                for (const auto& seg : segments) {
                    drawThickLine(window, seg.first, seg.second, 4.f, sf::Color::Black);
                }
            }

            for (const auto& comp : components) comp->draw(window, font);

            sf::RectangleShape searchBox(sf::Vector2f(300.f, 35.f));
            searchBox.setPosition({10.f, 10.f});
            searchBox.setFillColor(sf::Color::White);
            searchBox.setOutlineColor(isSearching ? sf::Color(255, 100, 100) : sf::Color(150, 150, 150));
            searchBox.setOutlineThickness(2.f);
            window.draw(searchBox);

            sf::Text searchText(font, (searchQuery.empty() && !isSearching) ? "Search part (e.g. SINE)" : searchQuery, 16);
            searchText.setPosition({20.f, 18.f});
            searchText.setFillColor(searchQuery.empty() && !isSearching ? sf::Color(150,150,150) : sf::Color::Black);
            window.draw(searchText);

            if (isSearching && !searchQuery.empty()) {
                float x = 10.f;
                float y = 50.f;
                std::string lq = searchQuery;
                std::transform(lq.begin(), lq.end(), lq.begin(), ::tolower);

                for (const auto& def : COMPONENT_DATABASE) {
                    std::string match = def.name + " " + def.desc;
                    std::transform(match.begin(), match.end(), match.begin(), ::tolower);
                    
                    if (lq == "all" || match.find(lq) != std::string::npos) {
                        sf::RectangleShape itemBg(sf::Vector2f(300.f, 40.f));
                        itemBg.setPosition({x, y});
                        itemBg.setFillColor(sf::Color(245, 245, 245));
                        if (itemBg.getGlobalBounds().contains(mousePos)) itemBg.setFillColor(sf::Color(255, 200, 200));
                        window.draw(itemBg);
                        
                        sf::Text itemText(font, def.name + " | " + def.desc, 14);
                        itemText.setPosition({x + 10.f, y + 10.f});
                        itemText.setFillColor(sf::Color::Black);
                        window.draw(itemText);
                        
                        y += 40.f;
                        if (y + 40.f > window.getSize().y) { 
                            y = 50.f;
                            x += 310.f;
                        }
                    }
                }
            }

            if (componentToPlace) {
                sf::RectangleShape ghost(componentToPlace->size);
                ghost.setPosition(mousePos - sf::Vector2f(componentToPlace->size.x / 2.f, componentToPlace->size.y / 2.f));
                ghost.setFillColor(sf::Color(255, 180, 180, 150));
                window.draw(ghost);
            }

            window.display();
        }
    }
};

int main() {
    SimulatorUI app;
    app.run();
    return 0;
}