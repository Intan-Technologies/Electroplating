#include "significantround.h"

#include <QtCore>
#include <cmath>


/* Round value to 3 significant figures */
double significantRound(double value)
{
    int factors = 0;
    double dividedValue = value;

    //Determine how many factors of 10 must be factored out to get to a number less than one
    while (abs(dividedValue) > 1) {
        dividedValue = dividedValue / 10;
        factors++;
    }

    //round to 3 decimals
    double roundedValue = dividedValue * 1000;
    roundedValue = round(roundedValue);
    roundedValue = roundedValue / 1000;
    double result = roundedValue;

    //Multiply by i (factors of 10) to get the final result
    for (int i = 0; i < factors; i++) {
        result = result * 10;
    }
    return result;
}
