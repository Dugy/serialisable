# Quick Preferences
A small header-only library for convenient saving of program preferences without boilerplate. It uses the [JSON format](https://en.wikipedia.org/wiki/JSON). It's designed for minising the amount of code used for saving and loading while keeping the saved file human-readable, it's not optimised for performance.

Normally, persistent preferences tend to have one method for saving that contains code for loading all the content and one method for saving it all. Also, there might be some validity checking. This allows saving it all in one method that calls overloads of one method to all members to be saved.

It contains a small JSON library to avoid a dependency on a library that is probably much larger than this one. It's not advised to be used for generic JSON usage.

## Usage

Have your classes inherit from the Serialisable class. They have to implement a `serialisation()` method that calls overloads of the `synch()` method that accepts name of the value in the file as first argument and the value (taken as reference) as the second one. If something needs to be processed before saving or after loading, the `saving()` method will return a bool value telling if it's being saved.

Supported types are `std::string`, arithmetic types (converted to `double` because of JSON's specifications), `bool`, any object derived from `Serialisable`, a `std::vector` of such objects or a `std::vector` of smart pointers to such objects (raw pointers will not be deleted if `load()` is called while the vector is not empty).

Default values should be set somewhere, because if `load()` does not find the specified file, it does not call the `serialisation()` method.

Missing keys will simply not write the value. Values of wrong types will throw.

```C++
struct Chapter : public Serialisable {
	std::string contents = "";
	std::string author = "Anonymous";

	virtual void serialisation() {
		synch("contents", contents);
		synch("author", author);
	}
};

struct Preferences : public Serialisable {
	std::string lastFolder = "";
	unsigned int lastOpen = 0;
	bool privileged = false;
	Chapter info;
	std::vector<Chapter> chapters;
	std::vector<std::shared_ptr<Chapter>> footnotes;
	std::vector<std::unique_ptr<Chapter>> addenda;

	virtual void serialisation() {
		synch("last_folder", lastFolder);
		synch("last_open", lastOpen);
		synch("privileged", privileged);
		synch("info", info);
		synch("chapters", chapters);
		synch("footnotes", footnotes);
		synch("addenda", addenda);
	}
};

//...

// Loading
Preferences prefs;
prefs.load("prefs.json");
  
// Saving
prefs.save("prefs.json");
```

It relies only on standard libraries, so you can use any C++11 compliant compiler to compile it.

## JSON library

The JSON library provided is only to avoid having additional dependencies. It's written to be short, its usage is prone to result in repetitive code. If you need JSON for something else, use a proper JSON library, like [the one written by Niels Lohmann](https://github.com/nlohmann/json), they are much more convenient.

If you really need to use it, for example if you are sure you will not use it much, here is an example:

``` C++
Serialisable::JSONobject testJson;
testJson.getObject()["file"] = std::make_shared<Serialisable::JSONstring>("test.json");
testJson.getObject()["number"] = std::make_shared<Serialisable::JSONdouble>(9);
testJson.getObject()["makes_sense"] = std::make_shared<Serialisable::JSONbool>(false);
std::shared_ptr<Serialisable::JSONarray> array = std::make_shared<Serialisable::JSONarray>();
for (int i = 0; i < 3; i++) {
	std::shared_ptr<Serialisable::JSONobject> obj = std::make_shared<Serialisable::JSONobject>();
	obj->getObject()["index"] = std::make_shared<Serialisable::JSONdouble>(i);
	std::shared_ptr<Serialisable::JSONobject> obj2 = std::make_shared<Serialisable::JSONobject>();
	obj->getObject()["contents"] = obj2;
	obj2->getObject()["empty"] = std::make_shared<Serialisable::JSONobject>();
	array->getVector().push_back(obj);
}
testJson.getObject()["data"] = array;
testJson.writeToFile("test.json");

std::shared_ptr<Serialisable::JSON> testReadJson = Serialisable::parseJSON("test.json");
testReadJson->getObject()["makes_sense"]->getBool() = true;
testReadJson->getObject()["number"]->getDouble() = 42;
testReadJson->writeToFile("test-reread.json");
```

The structure consists of JSON nodes of various types. They all have the same methods for accessing the contents returning references to the correct types (`getString()`, `getDouble()`, `getBool()`, `getObject()` and `getArray()`), but they are all virtual and only the correct one will not throw an exception. The type can be learned using the `type()` method. The interface class `Serialisable::JSON` is also the _null_ type.

The parser can parse incorrect code in some cases because some of the information in JSON files is redundant.
