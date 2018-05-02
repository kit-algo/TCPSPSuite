#ifndef PARAMETER_H
#define PARAMETER_H

#include <string>                       // for string, allocator
#include <vector>                       // for vector
#include <json.hpp>                     // for json

using json = nlohmann::json;

class Parameter {
public:

    Parameter(std::string name, int minValue, int maxValue, int stepSize);
    Parameter(std::string name, double minValue, double maxValue, double stepSize);
    Parameter(std::string name, bool value, bool fixed);
    Parameter(std::string name, json array);
    
    std::string getName();
    json getCurrentValue();
    bool isLastValue();
    void nextValue();

private:
    enum class type {integerValue, doubleValue, booleanValue, arrayValue};

    std::string parameterName;
    type valueType;
    
    int currentInt;
    int stepInt;
    int minInt;
    int maxInt;
    
    double currentDouble;
    double stepDouble;
    double minDouble;
    double maxDouble;
    
    bool currentBoolean;
    bool stepBoolean;
    bool maxBoolean;
    
    json arrayValues;
    size_t currentIndex;
    
};

#endif
