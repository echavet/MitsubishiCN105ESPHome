#pragma once
#include "Globals.h"



#define MAX_FUNCTION_CODE_COUNT 30

struct heatpumpFunctionCodes {
    bool valid[MAX_FUNCTION_CODE_COUNT];
    int code[MAX_FUNCTION_CODE_COUNT];
};




class heatpumpFunctions {
private:
    uint8_t raw[MAX_FUNCTION_CODE_COUNT];
    bool _isValid1;
    bool _isValid2;

    int getCode(uint8_t b);
    int getValue(uint8_t b);

public:
    heatpumpFunctions();

    bool isValid() const;

    // data must be 15 bytes
    void setData1(uint8_t* data);
    void setData2(uint8_t* data);
    void getData1(uint8_t* data) const;
    void getData2(uint8_t* data) const;

    void clear();

    int getValue(int code);
    bool setValue(int code, int value);

    heatpumpFunctionCodes getAllCodes();

    bool operator==(const heatpumpFunctions& rhs);
    bool operator!=(const heatpumpFunctions& rhs);
};
