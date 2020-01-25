#include <iostream>
#include <iterator>
#include "serialisable.hpp"

int main(int argc, char** argv) {
	if (argc != 2) {
		std::cout << "Usage: " << argv[0] << " file_name" << std::endl;
		return 1;
	}
	const std::string fileName = argv[1];
	const auto dotLocation = fileName.find_last_of('.');
	std::string name = (dotLocation != std::string::npos) ? fileName.substr(0, dotLocation) : fileName;
	if (dotLocation != std::string::npos && fileName.substr(dotLocation + 1) == "json") {
		std::ifstream input(fileName);
		if (!input.good()) {
			std::cerr << "Cannot read file: " << fileName << std::endl;
			return 2;
		}
		std::shared_ptr<Serialisable::JSON> json = Serialisable::parseJSON(input);
		input.close();
		json->writeToFile("readCheck.json");
		std::vector<uint8_t> outVector = json->condensed();
		std::string outputName = name + ".cjson";
		std::ofstream output(outputName);
		if (!output.good()) {
			std::cerr << "Cannot write file: " << outputName << std::endl;
			return 2;
		}
		for (uint8_t c : outVector) {
			output << c;
		}
	} else {
		std::ifstream input(fileName, std::ios::binary);
		if (!input.good()) {
			std::cerr << "Cannot read file: " << fileName << std::endl;
			return 2;
		}
		std::vector<uint8_t> inputVector(std::istreambuf_iterator<char>(input), {});
		input.close();
		std::shared_ptr<Serialisable::JSON> json = Serialisable::parseCondensed(inputVector);
		std::string outputName = name + ".json";
		std::ofstream output(outputName, std::ios::binary);
		if (!output.good()) {
			std::cerr << "Cannot write file: " << outputName << std::endl;
			return 2;
		}
		json->write(output);
	}
	return 0;
}
