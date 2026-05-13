#pragma once

struct BatteryState {
    int  percent;
    bool charging;
};

class Battery {
public:
    BatteryState read() const;
};
