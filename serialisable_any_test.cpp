#include "serialisable_any.hpp"
#include <string>
#include <iostream>

struct Mystery {
	int a = 3;
	std::string b = "I am not gay.";
	float c = 4.5;
	bool d = true;
	std::shared_ptr<std::string> e = nullptr;
	short int f = 13;
	double g = 14.34;
	struct {
		int a;
		double b;
	} h;
};

int main() {
	std::string source = "[15, \"High albedo, low roughness\", 17.424, false, null, 18, 123.214, [814, 241.134]]";
	Mystery made = readJsonObject<Mystery>(source);
	std::cout << "Member test: " << made.b << std::endl;
	Serialisable::JSON remade = writeJsonObject(made);
	std::cout << "Reserialised: " << remade << std::endl;
}
