#include <iostream>
#include "serialisable_brief.hpp"

enum DocumentType {
	BOOK = 1,
	ESSAY
};

struct Chapter : public SerialisableBrief {
	std::string contents = bound("contents");
	std::string author = bound("author").init("Anonymous");
};

struct Preferences : public SerialisableBrief {
	std::string lastFolder = bound("last_folder").init("");
	unsigned int lastOpen = bound("last_open").init(0);
	int daysUntilPublication = bound("days_until_publication", -5);
	uint64_t maxFilesAllowed = bound("max_files_allowed", UINT64_MAX);
	double relativeValue = bound("relative_value").init(0.45);
	bool privileged = bound("privileged", false);
	int reusableVariable = unbound(3);
	char reusableVariable2 = unbound().init('a');
	DocumentType documentType = bound("document_type", BOOK);
	Chapter info = bound("info");
	std::vector<Chapter> chapters = bound("chapters").init(3, Chapter());
	std::vector<std::shared_ptr<Chapter>> footnotes = bound("footnotes");
	std::vector<std::unique_ptr<Chapter>> addenda = bound("addenda");
	std::string* editorsNote = bound("editors_notes", nullptr);
};

int main() {
	Serialisable::JSONobject testJson;
	testJson.getObject()["file"] = std::make_shared<Serialisable::JSONstring>("test.json");
	testJson.getObject()["number"] = std::make_shared<Serialisable::JSONint>(9);
	testJson.getObject()["float_number"] = std::make_shared<Serialisable::JSONdouble>(9);
	testJson.getObject()["makes_sense"] = std::make_shared<Serialisable::JSONbool>(false);
	std::shared_ptr<Serialisable::JSONarray> array = std::make_shared<Serialisable::JSONarray>();
	for (int i = 0; i < 3; i++) {
		std::shared_ptr<Serialisable::JSONobject> obj = std::make_shared<Serialisable::JSONobject>();
		obj->getObject()["index"] = std::make_shared<Serialisable::JSONint>(i);
		std::shared_ptr<Serialisable::JSONobject> obj2 = std::make_shared<Serialisable::JSONobject>();
		obj->getObject()["contents"] = obj2;
		obj2->getObject()["empty"] = std::make_shared<Serialisable::JSONobject>();
		array->getVector().push_back(obj);
	}
	testJson.getObject()["data"] = array;
	testJson.writeToFile("test.json");

	std::shared_ptr<Serialisable::JSON> testReadJson = Serialisable::parseJSON("test.json");
	testReadJson->getObject()["makes_sense"]->getBool() = true;
	testReadJson->getObject()["number"]->getInt() = 42;
	testReadJson->getObject()["float_number"]->getDouble() = 4.9;
	testReadJson->writeToFile("test-reread.json");

	Preferences prefs;
	prefs.load("prefs.json");
	prefs.footnotes.push_back(std::make_shared<Chapter>());
	prefs.footnotes.back()->contents = "There will be a lot of footnotes";
	prefs.footnotes.back()->author = "Dugi";
	prefs.documentType = ESSAY;
	prefs.save("prefs.json");

	return 0;
}
