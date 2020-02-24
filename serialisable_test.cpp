#include <iostream>
#include "serialisable.hpp"

enum DocumentType {
	BOOK = 1,
	ESSAY
};

struct Chapter : public Serialisable {
	std::string contents = "";
	std::string author = "Anonymous";
	std::unordered_map<std::string, std::string> critique;

	virtual void serialisation() {
		synch("contents", contents);
		synch("author", author);
		synch("critique", critique);
	}
};

struct Preferences : public Serialisable {
	std::string lastFolder = "";
	unsigned int lastOpen = 0;
	int daysUntilPublication = -5;
	uint64_t maxFilesAllowed = UINT64_MAX;
	double relativeValue = 0.45;
	bool privileged = false;
	DocumentType documentType = BOOK;
	Chapter info;
	std::vector<Chapter> chapters;
	std::vector<std::shared_ptr<Chapter>> footnotes;
	std::vector<std::unique_ptr<Chapter>> addenda;
	std::shared_ptr<Serialisable::JSON> customValue;
	std::vector<uint8_t> raw;
#if __cplusplus > 201402L
	std::optional<std::string> critique;
#endif

	virtual void serialisation() {
		synch("last_folder", lastFolder);
		synch("last_open", lastOpen);
		synch("days_until_publication", daysUntilPublication);
		synch("max_files_allowed", maxFilesAllowed);
		synch("relative_value", relativeValue);
		synch("privileged", privileged);
		synch("document_type", documentType);
		synch("info", info);
		synch("chapters", chapters);
		synch("footnotes", footnotes);
		synch("addenda", addenda);
		synch("custom_value", customValue);
		synch("raw", raw);
#if __cplusplus > 201402L
		synch("critique", critique);
#endif
	}
};

int main() {
	Serialisable::JSON testJson;
	testJson.setObject()["file"] = "test.json";
	testJson["number"] = 9;
	testJson["float_number"] = 9.0;
	testJson["makes_sense"] = false;
	Serialisable::JSON array;
	array.setArray();
	for (int i = 0; i < 3; i++) {
		Serialisable::JSON obj;
		obj.setObject()["index"] = i;
		Serialisable::JSON obj2;
		obj2.setObject()["empty"] = Serialisable::JSON();
		obj["contents"] = obj2;
		array.push_back(obj);
	}
	testJson["data"] = array;
	testJson.save("test.json");

	Serialisable::JSON testReadJson = Serialisable::JSON::load("test.json");
	testReadJson["makes_sense"] = true;
	testReadJson["number"] = 42;
	testReadJson["float_number"] = 4.9;
	testReadJson.save("test-reread.json");

	Preferences prefs;
	prefs.load("prefs.json");
	prefs.footnotes.push_back(std::make_shared<Chapter>());
	prefs.footnotes.back()->contents = "There will be a lot of footnotes";
	prefs.footnotes.back()->author = "Dugi";
	prefs.documentType = ESSAY;
	prefs.raw.push_back(13);
	prefs.save("prefs.json");

	return 0;
}
