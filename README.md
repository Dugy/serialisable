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

It relies only on standard libraries, so you can use any C++14 compliant compiler to compile it.

## SerialisableBrief - write even less code

To write even less code for serialisation, you can use `SerialisableBrief`, a wrapper above `Serialisable`. It's in its separate header file.

It allows serialising with even less code:
```C++
struct Chapter : public SerialisableBrief {
	std::string contents = key("contents");
	std::string author = key("author").init("Anonymous");
};
```
It does the same as the `Chapter` class in the section above. The members must be initialised with the `key()` method, whose argument is the key of the field in JSON. The members can be optionally initialised using the `init()` method that takes any number of arguments that will be fed to the constructor (if not used, it will be default-initialised; brace-enclosed initialisers cannot be used). This can be shortened by adding any number of arguments after the first argument to the `key()` method, the additional arguments will be given to the constructor. This uses `Serialisable` to actually convert the variables to JSON, so it can serialise the same types as `Serialisable` can.

```C++
struct ChapterInfo : public SerialisableBrief {
	std::mutex lock = skip();
	int pages = key("pages").init(100);
	std::string summary = key("summary", "Some interesting stuff");
};
```

**Important:** if a member is *not* to be serialised, it has to be initialised with the `skip()` method (optionally taking constructor arguments; usable also for types that cannot be serialised by `Serialisable`). Otherwise, undefined behaviour is very likely to occur when serialising/deserialising the next member, without any warning. This applies also to any classes that inherit from it unless none of them uses any serialisation. Therefore, this should be used only for classes that hold data and don't have much other functionality. You have been warned.

The cost of this brevity is proneness to human errors, obscure code and lower performance, especially when constructing the objects. To avoid forgetting the `skip()` method, it's better to use `Serialisable` instead for more complex classes that aren't only for storing data. To avoid unnecessary inefficiency, it's recommended to copy or move the objects instead of creating new ones.

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

## A more condensed format

This is a binary markup language designed to use as little space as possible while keeping the same expressive power than JSON (and is thus directly convertible to JSON, though converting it back might wrongly guess the number of significant digits of floating point numbers without hints). It's also significantly more space efficient than BSON, although it's far from being so fast. Its only purpose is to take as little space as possible while keeping the versatility of JSON. It's not a compression algorithm, so it can be compressed afterwards to further reduce the size.

In most cases, it uses one byte of markup per value, the rest are long strings, large objects and large arrays that need two bytes. Small integers and boolean values are included in the markup bytes and take no additional space themselves. ASCII-only element names in objects are only as long as the strings themselves.

It has more types than JSON, but they are selected automatically for better space efficiency and translate to the same JSON. The types are marked with binary prefixes, while the prefixes may contain data themselves:
* **1xxxxxxx** - forms a 15 bit float with the next byte (almost half-precision), 1 bit is sign, 6 are exponent, 8 are mantissa, so the imprecision is about 0.2% and maximal value is in the order of ten power 9 *(total size is 2)*
* **011xxxxx** - string of size below 30, size is stored in the remaining bits *(total size is length + 1)*
* **01111110** - reserved
* **01111111** - long string, zero-terminated *(total size is length + 2)*
* **010xxxxx** - 5 bit signed integer, saved in the type *(total size is 1)*
* **00111xxx** - object whose member names must contain ASCII-symbols and objects with the same layout appear more than once in the JSON, first occurrence comes with a zero-terminated definition of all member names terminated by the most significant bit flipped, last three bits form the identifier *(total size of object is contents + 1 and once element names + 1)*
* **00111101** - reserved
* **00111110** - same, but identifier is the following byte *(total size of object is contents + 2 and once element names + 1)*
* **00111111** - same, but the identifier are the two following bytes *(total size of object is contents + 3 and once element names + 1)*
* **00110xxx** - small object with layout appearing only once, with max size up to 5 written in the type, with ASCII-only element names first and contents after *(total size is contents + element names + 1)*
* **00110110** - large object whose zero-terminated names list must contain ASCII-symbols only *(total size is of object is contents + element names + 2)*
* **00110111** - large, zero-terminated hashtable/object whose zero terminated member names do not contain only ASCII-symbols *(total size is contents + element names + number of elements + 2)*
* **0010xxxx** - array of size up to 14, size is a part of the type, size is stored in the type *(total size 1 + total size of contents)*
* **00101110** - reserved
* **00101111** - large, zero-terminated array of objects *(total size is 2 + total size of contents)*
* **0001xxxx** - forms a 12 bit signed integer with the following byte *(size is 2)*
* **00001111** - double *(size is 9)*
* **00001110** - float *(size is 5)*
* **00001101** - signed 64 bit integer *(size is 9)*
* **00001100** - unsigned 64 bit integer *(size is 9)*
* **00001011** - signed 32 bit integer *(size is 5)*
* **00001010** - unsigned 32 bit integer *(size is 5)*
* **00001001** - signed 16 bit integer *(size is 3)*
* **00001000** - unsigned 16 bit integer *(size is 3)*
* **000001xx** - reserved
* **00000011** - true *(size is 1)*
* **00000010** - false *(size is 1)*
* **00000001** - null *(size is 1)*
* **00000000** - zero termination, for terminating strings, objects/hashtables and arrays too large to have their sizes in the type byte *(size is counted to types that need it)*

It can be accessed using methods `serialiseCondensed()` and `deserialise()`, using data saved in a `std::vector<uint8_t>`.
