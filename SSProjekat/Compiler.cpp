#include "Compiler.h"
#include "UtilFunctions.h"
#include "Instruction.h"

#include <iostream>
#include <sstream>

using namespace std;

map<string, regex> Compiler::regexMap = {
	{"END", regex(".end")},
	{"ANYTHING", regex("")},
	{"LINEBREAK", regex("\\r")},
	{"LABEL", regex("^([a-zA-Z_][a-zA-Z0-9]*):$") },
	{"SECTION", regex("^\\.(text|data|bss|rodata)$")},
	{"DIRECTIVE", regex("^\\.(char|word|long|skip|align)$")},
	{"GLOBAL", regex("^\\.global$")},
	{"INSTRUCTION", regex("^(eq|ne|gt|al)(add|sub|mul|div|cmp|and|or|not|test|push|pop|call|iret|mov|shl|shr|ret|jmp)$")},
	{"ADDRESSING_SIGNS", regex("[\\&|\\*|\\[|\\$]")},


	{ "NUMBER", regex("^([0-9]+)$")},
	{ "ARITMETICAL", regex("^(eq|ne|gt|al)(add|sub|mul|div|and|or|not)$")},
	{ "LOGICAL", regex("^(eq|ne|gt|al)(cmp|test)$") },


	{ "OPERAND_DEC", regex("^([0-9]+)$") },
	{ "OPERAND_HEX", regex("^(0x[0-9abcdef]+)$") },
	{ "OPERAND_REG", regex("^r([0-7])$") },
	{ "OPERAND_REGSPEC", regex("^(pc|sp)$") },
	{ "OPERAND_SYMBOL", regex("^\\&([a-zA-Z_][a-zA-Z0-9]*)$") },
	{ "OPERAND_MEMDIR", regex("^([a-zA-Z_][a-zA-Z0-9]*)$") },
	{ "OPERAND_IMMADDR", regex("^\\*([0-9]+)$") },
	{ "OPERAND_IMMADDRHEX", regex("^\\*(0x[0-9abcdef]+)$") },
	{ "OPERAND_PCREL", regex("^\\$([a-zA-Z_][a-zA-Z0-9]*)$") },
	{ "OPERAND_REGINDPOM", regex("^r([0-7])\\[") },
};

Compiler::Compiler() {
	table = new SymbolTable();
	relocationTable = new RelocationSymbolTable();
	currentSection = "";
	locationCounter = 0;
	number = 5;

	generatedCode = {
		{".text", ""},
		{".data", ""},
		{".rodata", ""}
	};
}

Compiler::~Compiler() {
	delete table;
}


void Compiler::compile(ifstream &inFile, ofstream &outFile) {
	try{
		firstRun(inFile);
		table->print();
		for (int i = 0; i < sections.size(); i++) {
			Section s = sections.at(i);
			cout << s.getName() << " " << s.getLength() << endl;
		}
		secondRun(inFile);
	}
	catch (exception &e) {
		cout << e.what() << endl;
	}
}

void Compiler::firstRun(ifstream &inFile) {
	string line;
	while (getline(inFile, line)) {
		vector<string> words = UtilFunctions::split(line);

		for (vector<string>::size_type i = 0; i < words.size(); i++) {

			if (words[i] == "\n" || words[i] == "\r")break; //if end of line
			else if (words[i] == " ") continue; //if there are more than one spaces
			else if (words[i] == ".end") {
				//SAVE THE LAST SECTION
				Section* s = new Section(currentSection, 0, locationCounter);
				sections.push_back(*s);
				return;
			}

			else if (regex_search(words[i], regexMap["SECTION"])) {
				string labelName = words[i];

				Symbol* sym = table->get(labelName);
				if (sym != 0) throw new runtime_error("ERROR: Section can't be defined more than once!");

				//SAVE THE CURRENT SECTION
				if (currentSection != "") {
					Section* s = new Section(currentSection, 0, locationCounter);
					sections.push_back(*s);
				}

				//NEW SECTION
				currentSection = labelName;
				Symbol* newSym = new Symbol(labelName, currentSection, locationCounter, "local", UtilFunctions::getSectionNumber(labelName));
				table->put(labelName, newSym);
				locationCounter = 0;
				continue;
			}

			else if (regex_search(words[i], regexMap["LABEL"])) {
				string labelName = words[i].substr(0, words[i].size() - 1);

				Symbol* sym = table->get(labelName);
				if (sym != 0) {
					if (sym->getLocGlo() == "local") throw new runtime_error("ERROR: There can't be two or more symbols with the same name!");
					else {
						sym->setOffset(locationCounter);
						sym->setSection(currentSection);
						sym->setNumber(number);
						sym->setLocGlo("global");
					}
				}
				else {
					Symbol* newSym = new Symbol(labelName, currentSection, locationCounter, "local", number);
					number++;
					table->put(labelName, newSym);
				}
				continue;
			}

			else if (regex_search(words[i], regexMap["DIRECTIVE"])) {
				string name = words[i];
				if (name == ".skip" || name == ".align") {
					i++;
					int k=0;
					try {
						k = stoi(words[i]);
					}
					catch (exception e) {
						throw new runtime_error("ERROR: Invalid argument for directives .skip or .align!");
					}
					if (name == ".skip") locationCounter += k;
					else if (name == ".align") {
						if (k == 0) continue;
						if ((k & (k - 1)) == 0) {
							if (locationCounter / k * k != locationCounter) locationCounter = (locationCounter / k + 1) * k;
						}
						else throw new runtime_error("ERROR: Invalid argument for .skip directive, argument must be a power of 2");
					}
				}
				else if (name == ".char" || name == ".word" || name == ".long") {
					int size = UtilFunctions::getDirectiveSize(name);
					int k =words.size() - i - 1;
					locationCounter += k * size;
				}
				break;
			}

			else if (regex_search(words[i], regexMap["GLOBAL"])) {
				for (int k = i + 1; k < words.size(); k++) {
					string labelName = words[k];
					Symbol* sym = table->get(labelName);

					if (sym != 0) sym->setLocGlo("global");
					else {
						sym = new Symbol(labelName, "UND", locationCounter, "global", number);
						number++;
						table->put(labelName, sym);
					}

				}
			}

			else if (regex_search(words[i], regexMap["INSTRUCTION"])) {
				locationCounter += 2;
				for (int k = i + 1; k < words.size(); k++) {
					if ( regex_search(words[k], regexMap["ADDRESSING_SIGNS"]) || regex_search(words[k], regexMap["OPERAND_DEC"]) || regex_search(words[k], regexMap["OPERAND_HEX"])) {
						locationCounter += 2;
						break;
					}
				}
				break;
			}

			else continue;
		}

	}
	//SAVE THE LAST SECTION
	Section* s = new Section(currentSection, 0, locationCounter);
	sections.push_back(*s);
}

void Compiler::secondRun(ifstream &inFile) {
	string line;
	currentSection = "";
	locationCounter = 0;
	number = 5;

	while (getline(inFile, line)) {
		vector<string> words = UtilFunctions::split(line);
		for (vector<string>::size_type i = 0; i < words.size(); i++) {
			if (regex_search(words[i], regexMap["INSTRUCTION"])) {

				if (currentSection != ".text") {
					throw new runtime_error("ERROR: Instructions must be in .text section");
					return;
				}

				else if (regex_search(words[i], regexMap["ARITMETICAL"])) { //add, sub, mul, div, and, or, not
					if (words.size() != 3) {
						throw new runtime_error("ERROR: Wrong number of arguments for aritmetical operation");
					}
					
					string operation = "ARITMETICAL";
					string op1 = words[i + 1];
					string op2 = words[i + 2];
					string src = "";
					string dst = "";
					bool flag1 = false;
					bool flag2 = false;
					string value = "";

					process_first_operand(&operation, &op1, &src, &flag1, &value);
					process_second_operand(&operation, &op2, &dst, &flag2, &value);

					if (flag1 == true && flag2 == true)throw new runtime_error("ERROR: Only one operand can request aditional bytes to store data");

					string code = UtilFunctions::binaryToHexa(Instruction::instructions[words[i]]->getOpcode() + src + dst);
					generatedCode[currentSection] = generatedCode[currentSection] + code + value;

					locationCounter += 2;
					if (flag1 == true || flag2 == true)locationCounter += 2;

					break;
				}

				else if (regex_search(words[i], regexMap["LOGICAL"])) {
					if (words.size() != 3) {
						throw new runtime_error("ERROR: Wrong number of arguments for aritmetical operation");
					}

					string operation = "LOGICAL";
					string op1 = words[i + 1];
					string op2 = words[i + 2];
					string src = "";
					string dst = "";
					bool flag1 = false;
					bool flag2 = false;
					string value = "";

					process_first_operand(&operation, &op1, &src, &flag1, &value);
					process_second_operand(&operation, &op2, &dst, &flag2, &value);

					if (flag1 == true && flag2 == true)throw new runtime_error("ERROR: Only one operand can request aditional bytes to store data");

					string code = UtilFunctions::binaryToHexa(Instruction::instructions[words[i]]->getOpcode() + src + dst);
					generatedCode[currentSection] = generatedCode[currentSection] + code + value;

					locationCounter += 2;
					if (flag1 == true || flag2 == true)locationCounter += 2;

					break;
				
				}

				else if (words[i]=="eqiret" || words[i] == "neiret" || words[i] =="gtiret" || words[i] == "aliret") {
					string code = UtilFunctions::binaryToHexa(Instruction::instructions[words[i]]->getOpcode() + "0000000000");
					generatedCode[currentSection]= generatedCode[currentSection] + code;
					locationCounter += 2;
					break;
				}

				
			
			}

			if (regex_search(words[i], regexMap["SECTION"])) {
				locationCounter = 0;
				currentSection = words[i];
				continue;
			}

			else if (regex_search(words[i], regexMap["DIRECTIVE"])) {
				int size = UtilFunctions::getDirectiveSize(words[i]);
				string name = words[i];
				if (name == ".skip" || name == ".align") {
					i++;
					int k = 0;
					try {
						k = stoi(words[i]);
					}
					catch (exception e) {
						throw new runtime_error("ERROR: Invalid argument for directives .skip or .align!");
					}
					if (name == ".skip") locationCounter += k;
					else if (name == ".align") {
						if (k == 0) continue;
						if ((k & (k - 1)) == 0) {
							if (locationCounter / k * k != locationCounter) locationCounter = (locationCounter / k + 1) * k;
						}
						else throw new runtime_error("ERROR: Invalid argument for .skip directive, argument must be a power of 2");
					}
				}

				else if (name == ".char" || name == ".word" || name == ".long") {
					int size = UtilFunctions::getDirectiveSize(name);
					for (int k = i + 1; k < words.size(); k++) {
						//IF IT IS A NUMBER, ONLY DECIMAL FOR NOW
						if (regex_search(words[i], regexMap["NUMBER"])) {	
							int val = 0;
							try {
								val = stoi(words[i]);
							}
							catch (exception e) {
								throw new runtime_error("ERROR: Unexpected conversion error!");
							}
							string code = UtilFunctions::generateCode(val, size);
							generatedCode[currentSection]+=code;
							locationCounter += size;
						}
						//IF IT IS A SYMBOL
						else {										
							Symbol * sym = table->get(words[i]);
							if (sym == 0) {
								throw new runtime_error("ERROR: Symbol not defined");
							}
							else {
								if (sym->getLocGlo() == "global") {
									string code = UtilFunctions::generateCode(0, size);
									generatedCode[currentSection] += code;

									string address = UtilFunctions::decimalToHexa(locationCounter);
									RelocationSymbol* rels = new RelocationSymbol(address, false, sym->getNumber());
									relocationTable->put(currentSection, rels);
								}
								else if (sym->getLocGlo() == "local") {
									int offset = sym->getOffset();
									string code = UtilFunctions::generateCode(offset, size);
									generatedCode[currentSection] +=code;

									string address = UtilFunctions::decimalToHexa(locationCounter);
									RelocationSymbol *rels = new RelocationSymbol(address, false, UtilFunctions::getSectionNumber(currentSection));
									relocationTable->put(currentSection, rels);
									locationCounter += size;
								}
							}
						}
					}
				
				} //for else .char .word .long



			}
			else continue;
		}
	}

}

void Compiler::process_first_operand(string* operation, string* op1, string* src, bool* flag1, string* value) {
	string addressing = findAddressing(*op1);

	if (addressing == "immediateDec" || addressing == "immediateHex") {
		if (*operation == "ARITMETICAL") throw new runtime_error("ERROR: First operand can't be a immediate value in artihmetical operations!");
		*src = "00000";
		*flag1 = true;
		
		if (addressing == "immediateDec") { 
			int v = stoi(*op1);
			*value = UtilFunctions::generateCode(v, 2); 
		}
		else {
			string opp = *op1;
			*value = opp[2] + opp[3] + opp[0] + opp[1]; //little endian processor
		}
	}

	else if(addressing == "regDir" || addressing == "regDirSpec"){
		if (addressing == "regDir") {
			string opp = *op1;
			int regNum = (int)opp[1]; //register number
			string reg = UtilFunctions::decimalToBinary(regNum);
			*src = "01" + reg;
		}
		else {
			string opp = *op1;
			if (opp == "sp") *src = "01110"; //r6
			else if (opp == "pc") *src = "01111"; //r7
		}
	}

	else if (addressing == "immAddr" || addressing == "immAddrHex") {
		*src = "10000";
		*flag1 = true;

		if (addressing == "immAddr") {
			string opp = *op1;
			string num = opp.substr(1, opp.size());
			int v = stoi(num);
			*value = UtilFunctions::generateCode(v, 2);
		}
		else {
			string opp = *op1;
			*value = opp[2] + opp[3] + opp[0] + opp[1];
		}
	}

	else if (addressing == "memDir") {
		*src = "10000";
		*flag1 = true;
		string symName = *op1;
		Symbol * sym = table->get(symName);
		if (sym == 0) {
			throw new runtime_error("ERROR: Unexpected error, there is no symbol in the table");
		}
		else {
			if (sym->getLocGlo() == "local") {
				*value = UtilFunctions::generateCode(sym->getOffset(), 2);
				//RELOCATION SYMBOL
				string address = UtilFunctions::decimalToHexa(locationCounter + 2);
				RelocationSymbol* rels = new RelocationSymbol(address, false, UtilFunctions::getSectionNumber(currentSection));
				relocationTable->put(currentSection, rels);
			}
			else {  //global
				*value = "0000";
				//RELOCATION SYMBOL
				string address = UtilFunctions::decimalToHexa(locationCounter + 2);
				RelocationSymbol* rels = new RelocationSymbol(address, false, sym->getNumber());
				relocationTable->put(currentSection, rels);
			}
		}
	}

	else if (addressing == "symVal") {
		if (*operation == "ARITMETICAL") throw new runtime_error("ERROR: First operand can't be a immediate value in artihmetical operations!");
		*src = "00000";
		*flag1 = true;
		string opp = *op1;
		string symName = opp.substr(1, opp.size());
		Symbol* sym = table->get(symName);

		if (sym == 0) {
			throw new runtime_error("ERROR: Unexpected error, there is no symbol in the table");
		}
		else {
			if (sym->getLocGlo() == "local") {
				*value = UtilFunctions::generateCode(sym->getOffset(), 2);
				//RELOCATION SYMBOL
				string address = UtilFunctions::decimalToHexa(locationCounter + 2);
				RelocationSymbol* rels = new RelocationSymbol(address, false, UtilFunctions::getSectionNumber(currentSection));
				relocationTable->put(currentSection, rels);
			}
			else {  //global
				*value = "0000";
				//RELOCATION SYMBOL
				string address = UtilFunctions::decimalToHexa(locationCounter + 2);
				RelocationSymbol* rels = new RelocationSymbol(address, false, sym->getNumber());
				relocationTable->put(currentSection, rels);
			}
		
		}
	}

	else if (addressing == "regIndPom") {
		*flag1 = true;
		string opp = *op1;

		string pom = "";
		int regNum;
		bool found = false;
		for (int i = 0; i < opp.size(); i++) {
			if (opp[i] == '[') {
				found = true;
				regNum = (int)opp[i - 1];
				continue;
			}
			if (opp[i] == ']')found = false;
			if (found == true) {
				pom += opp[i];
			}
		}

		string reg = UtilFunctions::decimalToBinary(regNum);
		*src = "11" + reg;

		if (regex_search(pom, regexMap["OPERAND_DEC"]) || regex_search(pom, regexMap["OPERAND_HEX"])) {
			if (regex_search(pom, regexMap["OPERAND_DEC"])) {
				int v = stoi(pom);
				*value = UtilFunctions::generateCode(v, 2);
			}
			else {
				*value = pom[2] + pom[3] + pom[0] + pom[1];
			}
		}

		else if(regex_search(pom, regexMap["OPERAND_MEMDIR"])) { //regex is the same!!!!!!!!
			Symbol* sym = table->get(pom);
			if (sym == 0) {
				throw new runtime_error("ERROR: Unexpected error, there is no symbol in the table!");
			}
			else {
				if (sym->getLocGlo() == "local") {
					*value = UtilFunctions::generateCode(sym->getOffset(), 2);
					//RELOCATION SYMBOL
					string address = UtilFunctions::decimalToHexa(locationCounter + 2);
					RelocationSymbol* rels = new RelocationSymbol(address, false, UtilFunctions::getSectionNumber(currentSection));
					relocationTable->put(currentSection, rels);
				}
				else {  //global
					*value = "0000";
					//RELOCATION SYMBOL
					string address = UtilFunctions::decimalToHexa(locationCounter + 2);
					RelocationSymbol* rels = new RelocationSymbol(address, false, sym->getNumber());
					relocationTable->put(currentSection, rels);
				}
			}
		}
		
		else throw new runtime_error("ERROR: Bad syntax for regIndPom addressing");
	}

	else if (addressing == "pcrel") {
		*src = "00111";
		*flag1 = true;
		string opp = *op1;
		string symName = opp.substr(1, opp.size());
		Symbol* sym = table->get(symName);

		if (sym == 0) {
			throw new runtime_error("ERROR: Unexpected error, there is no symbol in the table");
		}
		else {
			if (sym->getLocGlo() == "local") {
				*value = "?";
				string address = UtilFunctions::decimalToHexa(locationCounter + 2);
				RelocationSymbol* rels = new RelocationSymbol(address, true, UtilFunctions::getSectionNumber(currentSection));
				relocationTable->put(currentSection, rels);
			}
			else {
				*value = "?";
				string address = UtilFunctions::decimalToHexa(locationCounter + 2);
				RelocationSymbol* rels = new RelocationSymbol(address, false, sym->getNumber());
				relocationTable->put(currentSection, rels);
			}
		}
	}

	else {
		throw new runtime_error("ERROR: Can not find specified addressing");
	}
	
}

void Compiler::process_second_operand(string* operation, string* op2, string* dst, bool* flag2, string* value) {
	string addressing = findAddressing(*op2);

	if (addressing == "immediateDec" || addressing == "immediateHex") {
		*dst = "00000";
		*flag2 = true;

		if (addressing == "immediateDec") {
			int v = stoi(*op2);
			*value = UtilFunctions::generateCode(v, 2);
		}
		else {
			string opp = *op2;
			*value = opp[2] + opp[3] + opp[0] + opp[1];
		}
	}

	else if (addressing == "regDir" || addressing == "regDirSpec") {
		if (addressing == "regDir") {
			string opp = *op2;
			int regNum = (int)opp[1]; //register number
			string reg = UtilFunctions::decimalToBinary(regNum);
			*dst = "01" + reg;
		}
		else {
			string opp = *op2;
			if (opp == "sp") *dst = "01110"; //r6
			else if (opp == "pc") *dst = "01111"; //r7
		}
	}

	else if (addressing == "immAddr" || addressing == "immAddrHex") {
		*dst = "10000";
		*flag2 = true;

		if (addressing == "immAddr") {
			string opp = *op2;
			string num = opp.substr(1, opp.size());
			int v = stoi(num);
			*value = UtilFunctions::generateCode(v, 2);
		}
		else {
			string opp = *op2;
			*value = opp[2] + opp[3] + opp[0] + opp[1];
		}
	}

	else if (addressing == "memDir") {
		*dst = "10000";
		*flag2 = true;
		string symName = *op2;
		Symbol * sym = table->get(symName);
		if (sym == 0) {
			throw new runtime_error("ERROR: Unexpected error, there is no symbol in the table");
		}
		else {
			if (sym->getLocGlo() == "local") {
				*value = UtilFunctions::generateCode(sym->getOffset(), 2);
				//RELOCATION SYMBOL
				string address = UtilFunctions::decimalToHexa(locationCounter + 2);
				RelocationSymbol* rels = new RelocationSymbol(address, false, UtilFunctions::getSectionNumber(currentSection)); //section entry number
				relocationTable->put(currentSection, rels);
			}
			else {  //global
				*value = "0000";
				//RELOCATION SYMBOL
				string address = UtilFunctions::decimalToHexa(locationCounter + 2);
				RelocationSymbol* rels = new RelocationSymbol(address, false, sym->getNumber()); //entry number
				relocationTable->put(currentSection, rels);
			}
		}

	}

	else if (addressing == "symVal") {
		if (*operation == "ARITMETICAL") throw new runtime_error("ERROR: First operand can't be a immediate value in artihmetical operations!");
		*dst = "00000";
		*flag2 = true;
		string opp = *op2;
		string symName = opp.substr(1, opp.size());
		Symbol* sym = table->get(symName);

		if (sym == 0) {
			throw new runtime_error("ERROR: Unexpected error, there is no symbol in the table");
		}
		else {
			if (sym->getLocGlo() == "local") {
				*value = UtilFunctions::generateCode(sym->getOffset(), 2);
				//RELOCATION SYMBOL
				string address = UtilFunctions::decimalToHexa(locationCounter + 2);
				RelocationSymbol* rels = new RelocationSymbol(address, false, UtilFunctions::getSectionNumber(currentSection));
				relocationTable->put(currentSection, rels);
			}
			else {  //global
				*value = "0000";
				//RELOCATION SYMBOL
				string address = UtilFunctions::decimalToHexa(locationCounter + 2);
				RelocationSymbol* rels = new RelocationSymbol(address, false, sym->getNumber());
				relocationTable->put(currentSection, rels);
			}

		}
	}

	else if (addressing == "regIndPom") {
		*flag2 = true;
		string opp = *op2;

		string pom = "";
		int regNum;
		bool found = false;
		for (int i = 0; i < opp.size(); i++) {
			if (opp[i] == '[') {
				found = true;
				regNum = (int)opp[i - 1];
			}
			if (opp[i] == ']')found = false;
			if (found == true) {
				pom += opp[i];
			}
		}

		string reg = UtilFunctions::decimalToBinary(regNum);
		*dst = "11" + reg;

		if (regex_search(pom, regexMap["OPERAND_DEC"]) || regex_search(pom, regexMap["OPERAND_HEX"])) {
			if (regex_search(pom, regexMap["OPERAND_DEC"])) {
				int v = stoi(pom);
				*value = UtilFunctions::generateCode(v, 2);
			}
			else {
				*value = pom[2] + pom[3] + pom[0] + pom[1];
			}
		}

		else if (regex_search(pom, regexMap["OPERAND_MEMDIR"])) { //regex is the same!!!!!!!!
			Symbol* sym = table->get(pom);
			if (sym == 0) {
				throw new runtime_error("ERROR: Unexpected error, there is no symbol in the table!");
			}
			else {
				if (sym->getLocGlo() == "local") {
					*value = UtilFunctions::generateCode(sym->getOffset(), 2);
					//RELOCATION SYMBOL
					string address = UtilFunctions::decimalToHexa(locationCounter + 2);
					RelocationSymbol* rels = new RelocationSymbol(address, false, UtilFunctions::getSectionNumber(currentSection));
					relocationTable->put(currentSection, rels);
				}
				else {  //global
					*value = "0000";
					//RELOCATION SYMBOL
					string address = UtilFunctions::decimalToHexa(locationCounter + 2);
					RelocationSymbol* rels = new RelocationSymbol(address, false, sym->getNumber());
					relocationTable->put(currentSection, rels);
				}
			}
		}
		
		else throw new runtime_error("ERROR: Bad syntax for regIndPom addressing");
	}

	else if (addressing == "pcrel") {
		
	}

	else {
		throw new runtime_error("ERROR: Can not find specified addressing");
	}

}

string Compiler::findAddressing(string op) {
	if (regex_search(op, regexMap["OPERAND_DEC"])) {
		return "immediateDec";
	}
	else if (regex_search(op, regexMap["OPERAND_HEX"])) {
		return "immediateHex";
	}

	else if (regex_search(op, regexMap["OPERAND_REG"])) {
		return "regDir";
	}
	else if (regex_search(op, regexMap["OPERAND_REGSPEC"])) {
		return "regDirSpec";
	}

	else if (regex_search(op, regexMap["OPERAND_SYMBOL"])) {
		return "symVal";
	}

	else if (regex_search(op, regexMap["OPERAND_MEMDIR"])) {
		return "memDir";
	}

	else if (regex_search(op, regexMap["OPERAND_IMMADDR"])) {
		return "immAddr";
	}
	else if (regex_search(op, regexMap["OPERAND_IMMADDRHEX"])) {
		return "immAddrHex";
	}

	else if (regex_search(op, regexMap["OPERAND_REGINDPOM"])) {
		return "regIndPom";
	}

	else if (regex_search(op, regexMap["OPERAND_PCREL"])) {
		return "pcrel";
	}

	else return "not found";

}
