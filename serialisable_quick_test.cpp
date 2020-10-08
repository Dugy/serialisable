#include <iostream>
#include "serialisable_quick.hpp"

struct InnerMystery : SerialisableQuick<InnerMystery> {
	uint8_t a = 13;
	int b = key("the constant");
	int8_t c = -13;
	std::string d;
	float e = key("magic number") = 13.37;
};

struct Mystery : SerialisableQuick<Mystery> {
	int a = 3;
	std::string b = key("the string") = "Nice to see this actually printed";
	float c = 4.5;
	bool d = true;
	std::shared_ptr<std::string> e = nullptr;
	short int f = key("the short integer") = 13;
	double g = 14.34;
	InnerMystery h = key("Seek treasures within yourself... so deep, yet made in 10 seconds");
};

struct OuterMystery : SerialisableQuick<OuterMystery> {
	int8_t a = 13;
	int8_t b = key("lucky number") = a;
	int8_t c = -13;
	int8_t d = key("so negative") = -13;
	bool e = true;
	double f = key("could be leetspeak") = 133.759343;
	std::shared_ptr<Mystery> g = key("This is no recursion") = std::make_shared<Mystery>();
	int8_t h;
	unsigned short int i = key("current in mA") = 3.4;
	int o;
	bool p;
	short int q = key("who knows what this will be");
	int8_t r = key("will this be the last?");
};

int main() {
	std::string source = "{\"the string\": \"Lucretius was lucky\", \"the short integer\": 18}";
	OuterMystery made;
	made.fromJson(Serialisable::JSON::fromString(source));
	std::cout << "Member test: " << made.b << std::endl;
	std::string remade = made.toJson().toString();
	std::cout << "Reserialised: " << remade << std::endl;
	
}
