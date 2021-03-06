#ifndef UTILFUNCTIONS_H
#define UTILFUNCTIONS_H

#include <iostream>
#include <string>
#include <fstream>
#include <math.h>
#include <bitset>
#include <vector>
#include <sstream>

using namespace std;

class UtilFunctions{
public:
	static vector<string> split(string line);
	static int getDirectiveSize(string directive);
	static int getSectionNumber(string section);

	static string decimalToBinary(int number);
	static string binaryToHexa(string binary);
	static string decimalToHexa(int number);
	
	static string generateCode(int, int);

	static int hexToDecimal(string num);
	static string hexToBinary(string hex);
	static int binaryToDec(string bin);
};

#endif

