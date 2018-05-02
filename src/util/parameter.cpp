#include "parameter.hpp"

Parameter::Parameter(std::string name, int minValue, int maxValue, int stepSize) 
 : parameterName(name), valueType(type::integerValue), currentInt(minValue), 
   stepInt(stepSize), minInt(minValue), maxInt(maxValue)
{
}

Parameter::Parameter(std::string name, double minValue, double maxValue, double stepSize) 
 : parameterName(name), valueType(type::doubleValue), currentDouble(minValue), 
   stepDouble(stepSize), minDouble(minValue), maxDouble(maxValue)
{
}

Parameter::Parameter(std::string name, bool minValue, bool fixed)
 : parameterName(name), valueType(type::booleanValue), currentBoolean(minValue), 
   stepBoolean(!fixed), maxBoolean(minValue ^ !fixed)
{
}
 
Parameter::Parameter(std::string name, json array)
 : parameterName(name), valueType(type::arrayValue), arrayValues(array), currentIndex(0)
{
}

std::string
Parameter::getName()
{
    return parameterName;
}
    
json 
Parameter::getCurrentValue()
{
    json j;
    switch (valueType)
    {
        case type::integerValue:
            j[parameterName] = currentInt;
            break;
        case type::doubleValue:
            j[parameterName] = currentDouble;
            break;
        case type::booleanValue:
            j[parameterName] = currentBoolean;
            break;
        case type::arrayValue:
            j[parameterName] = arrayValues[currentIndex];
            break;
        default:
            break;
    }
    return j;
}

bool
Parameter::isLastValue()
{
    switch (valueType)
    {
        case type::integerValue:
            return currentInt >= maxInt;
        case type::doubleValue:
            return currentDouble >= maxDouble; //TODO epsilon check?
        case type::booleanValue:
            return currentBoolean == maxBoolean;
        case type::arrayValue:
            return currentIndex + 1 == arrayValues.size();
        default:
            return true;
    }
}

void
Parameter::nextValue()
{
    switch (valueType)
    {
        case type::integerValue:
            if (currentInt >= maxInt) {
                currentInt = minInt;
            } else {
                currentInt = std::min(maxInt, currentInt + stepInt);
            }
            break;
        case type::doubleValue:
            if (currentDouble >= maxDouble) {
                currentDouble = minDouble;
            } else {
                currentDouble = std::min(maxDouble, currentDouble + stepDouble);
            }
            break;
        case type::booleanValue:
            currentBoolean ^= stepBoolean;
            break;
        case type::arrayValue:
            currentIndex += 1;
            currentIndex %= arrayValues.size();
            break;
        default:
            break;
    }
}